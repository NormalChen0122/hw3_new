#include "accelerometer_handler.h"
#include "config.h"
#include "magic_wand_model_data.h"

#include "tensorflow/lite/c/common.h"
#include "tensorflow/lite/micro/kernels/micro_ops.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"
#include "uLCD_4DGL.h"
#include "mbed_rpc.h"

#include "mbed.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "stm32l475e_iot01_accelero.h"
using namespace std::chrono;

// GLOBAL VARIABLES
WiFiInterface *wifi;

volatile int message_num = 0;
volatile int arrivedcount = 0;
volatile bool closed = false;

const char* topic_0 = "mode_0";
const char* topic_1 = "mode_1";
double ori_z;
char need_to_transmit[100];
DigitalOut led1(LED1);
DigitalOut led2(LED2);
DigitalOut led3(LED3);

MQTT::Client<MQTTNetwork, Countdown> *p_client;

// the functions
int gesture_UI_mode(void);
void qcall_gesture_UI_mode(Arguments *in, Reply *out);
RPCFunction rpc_qcall_gesture_UI_mode(&qcall_gesture_UI_mode, "qcall_gesture_UI_mode");
void stop_gesture_UI_mode(Arguments *in, Reply *out);
RPCFunction rpc_stop_gesture_UI_mode(&stop_gesture_UI_mode, "stop_gesture_UI_mode");
void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client, char *need_to_transmit, const char *topic);

void angle_detection_mode(void);
void qcall_angle_detection_mode(Arguments *in, Reply *out);
RPCFunction rpc_qcall_angle_detection_mode(&qcall_angle_detection_mode, "qcall_angle_detection_mode");

void stop_angle_detection_mode(Arguments *in, Reply *out);
RPCFunction rpc_stop_angle_detection_mode(&stop_angle_detection_mode, "stop_angle_detection_mode");

int PredictGesture(float* output);
InterruptIn button(USER_BUTTON);
// void rise_userb(MQTT::Client<MQTTNetwork, Countdown>* client);
void exe_rise_userb(MQTT::Client<MQTTNetwork, Countdown>* client);
uLCD_4DGL uLCD(D1, D0, D2); // serial tx, serial rx, reset pin;

// Create an area of memory to use for input, output, and intermediate arrays.
// The size of this will depend on the model you're using, and may need to be
// determined by experimentation.
constexpr int kTensorArenaSize = 60 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
char done_mode0;
char done_mode1;
int angle_sel;

Thread mqtt_thread(osPriorityNormal);
EventQueue mqtt_queue;
Thread func_thread(osPriorityNormal);
EventQueue func_queue;

BufferedSerial pc(USBTX, USBRX);

int main(int argc, char* argv[]) {
   //The mbed RPC classes are now wrapped to create an RPC enabled version - see RpcClasses.h so don't add to base class
   // receive commands, and send back the responses
   char buf[256], outbuf[256];

   // initialize the LED
   led1 = 0;
   led2 = 0;
   led3 = 0;


   wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
            printf("ERROR: No WiFiInterface found.\r\n");
            return -1;
    }


    printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
            printf("\nConnection error: %d\r\n", ret);
            return -1;
    }


    NetworkInterface* net = wifi;
    MQTTNetwork mqttNetwork(net);
    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    p_client = &client;

    //TODO: revise host to your IP
    const char* host = "192.168.58.120";
    printf("Connecting to TCP network...\r\n");

    SocketAddress sockAddr;
    sockAddr.set_ip_address(host);
    sockAddr.set_port(1883);

    printf("address is %s/%d\r\n", (sockAddr.get_ip_address() ? sockAddr.get_ip_address() : "None"),  (sockAddr.get_port() ? sockAddr.get_port() : 0) ); //check setting

    int rc = mqttNetwork.connect(sockAddr);//(host, 1883);
    if (rc != 0) {
            printf("Connection error.");
            return -1;
    }
    printf("Successfully connected!\r\n");

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "Mbed";

    if ((rc = p_client->connect(data)) != 0){
            printf("Fail to connect MQTT\r\n");
    }

    uLCD.cls();
    uLCD.color(WHITE);
    uLCD.locate(0,0);
    uLCD.printf("run python code"); //Default Green on black text




   
   FILE *devin = fdopen(&pc, "r");
   FILE *devout = fdopen(&pc, "w");
   mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));
   func_thread.start(callback(&func_queue, &EventQueue::dispatch_forever));
   button.rise(mqtt_queue.event(exe_rise_userb, p_client)); // attach the address of the flip function to the rising edge


   while(1) {
      memset(buf, 0, 256);      // clear buffer

      for(int i=0; ; i++) {
            char recv = fgetc(devin);
            if (recv == '\n') {
               printf("\r\n");
               break;
            }
            buf[i] = fputc(recv, devout);
      }
      //Call the static call method on the RPC class
      RPC::call(buf, outbuf);
      printf("%s\r\n", outbuf);
   }
  
}

// Return the result of the last prediction
int PredictGesture(float* output) {
  // How many times the most recent gesture has been matched in a row
  static int continuous_count = 0;
  // The result of the last prediction
  static int last_predict = -1;

  // Find whichever output has a probability > 0.8 (they sum to 1)
  int this_predict = -1;
  for (int i = 0; i < label_num; i++) {
    if (output[i] > 0.8) this_predict = i; // decide what gesture it is
  }

  // No gesture was detected above the threshold
  if (this_predict == -1) {
    continuous_count = 0;
    last_predict = label_num;
    return label_num;
  }

  if (last_predict == this_predict) {
    continuous_count += 1;
  } else {
    continuous_count = 0;
  }
  last_predict = this_predict;

  // If we haven't yet had enough consecutive matches for this gesture,
  // report a negative result
  if (continuous_count < config.consecutiveInferenceThresholds[this_predict]) {
    return label_num;
  }
  // Otherwise, we've seen a positive result, so clear all our variables
  // and report it
  continuous_count = 0;
  last_predict = -1;

  return this_predict;
}


void qcall_gesture_UI_mode(Arguments *in, Reply *out){
  func_queue.call(&gesture_UI_mode);
}

int gesture_UI_mode(void)
{
  // Whether we should clear the buffer next time we fetch data
  bool should_clear_buffer = false;
  bool got_data = false;

  // The gesture index of the prediction
  int gesture_index;

  // Set up logging.
  static tflite::MicroErrorReporter micro_error_reporter;
  tflite::ErrorReporter* error_reporter = &micro_error_reporter;

  // Map the model into a usable data structure. This doesn't involve any
  // copying or parsing, it's a very lightweight operation.
  const tflite::Model* model = tflite::GetModel(g_magic_wand_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    error_reporter->Report(
        "Model provided is schema version %d not equal "
        "to supported version %d.",
        model->version(), TFLITE_SCHEMA_VERSION);
    return -1;
  }

  // Pull in only the operation implementations we need.
  // This relies on a complete list of all the ops needed by this graph.
  // An easier approach is to just use the AllOpsResolver, but this will
  // incur some penalty in code space for op implementations that are not
  // needed by this graph.
  static tflite::MicroOpResolver<6> micro_op_resolver;
  micro_op_resolver.AddBuiltin(
      tflite::BuiltinOperator_DEPTHWISE_CONV_2D,
      tflite::ops::micro::Register_DEPTHWISE_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_MAX_POOL_2D,
                               tflite::ops::micro::Register_MAX_POOL_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_CONV_2D,
                               tflite::ops::micro::Register_CONV_2D());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_FULLY_CONNECTED,
                               tflite::ops::micro::Register_FULLY_CONNECTED());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_SOFTMAX,
                               tflite::ops::micro::Register_SOFTMAX());
  micro_op_resolver.AddBuiltin(tflite::BuiltinOperator_RESHAPE,
                               tflite::ops::micro::Register_RESHAPE(), 1);

  // Build an interpreter to run the model with
  static tflite::MicroInterpreter static_interpreter(
      model, micro_op_resolver, tensor_arena, kTensorArenaSize, error_reporter);
  tflite::MicroInterpreter* interpreter = &static_interpreter;

  // Allocate memory from the tensor_arena for the model's tensors
  interpreter->AllocateTensors();

  // Obtain pointer to the model's input tensor
  TfLiteTensor* model_input = interpreter->input(0);
  if ((model_input->dims->size != 4) || (model_input->dims->data[0] != 1) ||
      (model_input->dims->data[1] != config.seq_length) ||
      (model_input->dims->data[2] != kChannelNumber) ||
      (model_input->type != kTfLiteFloat32)) {
    error_reporter->Report("Bad input tensor parameters in model");
    return -1;
  }

  int input_length = model_input->bytes / sizeof(float);

  TfLiteStatus setup_status = SetupAccelerometer(error_reporter);
  if (setup_status != kTfLiteOk) {
    error_reporter->Report("Set up failed\n");
    return -1;
  }

  error_reporter->Report("Set up successful...\n");

  uLCD.cls();
  uLCD.text_width(2); //4X size text
  uLCD.text_height(2);
  uLCD.color(RED);
  uLCD.locate(0,0);
  uLCD.printf("40 deg"); //Default Green on black text
  uLCD.color(BLUE);
  uLCD.locate(0,1);
  uLCD.printf("50 deg"); //Default Green on black text
  uLCD.locate(0,2);
  uLCD.printf("60 deg"); //Default Green on black text
  uLCD.locate(0,3);
  uLCD.printf("70 deg"); //Default Green on black text
  uLCD.locate(0,4);
  uLCD.printf("80 deg"); //Default Green on black text


  // parameter initialize
  done_mode0 = 0;
  angle_sel = 40;
  led1 = 1;
  while (!done_mode0) {

    // Attempt to read new data from the accelerometer
    got_data = ReadAccelerometer(error_reporter, model_input->data.f,
                                 input_length, should_clear_buffer);

    // If there was no new data,
    // don't try to clear the buffer again and wait until next time
    if (!got_data) {
      should_clear_buffer = false;
      continue;
    }

    // Run inference, and report any error
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
      error_reporter->Report("Invoke failed on index: %d\n", begin_index);
      continue;
    }

    // Analyze the results to obtain a prediction
    gesture_index = PredictGesture(interpreter->output(0)->data.f);

    // Clear the buffer next time we read data
    should_clear_buffer = gesture_index < label_num;

    // Produce an output (print the outcome)


    if (gesture_index < label_num) {
      error_reporter->Report(config.output_message[gesture_index]);
      if (gesture_index == 1 || gesture_index == 0) {
        if (angle_sel > 40) angle_sel -= 10;
      }
      if (gesture_index == 2) {
        if (angle_sel < 80) angle_sel += 10;
      }
      uLCD.cls();
      // ULCD
      if (angle_sel == 40) {
        uLCD.text_width(2); //4X size text
        uLCD.text_height(2);
        uLCD.color(RED);
        uLCD.locate(0,0);
        uLCD.printf("40 deg"); //Default Green on black text
        uLCD.color(BLUE);
        uLCD.locate(0,1);
        uLCD.printf("50 deg"); //Default Green on black text
        uLCD.locate(0,2);
        uLCD.printf("60 deg"); //Default Green on black text
        uLCD.locate(0,3);
        uLCD.printf("70 deg"); //Default Green on black text
        uLCD.locate(0,4);
        uLCD.printf("80 deg"); //Default Green on black text
      }
      else if (angle_sel == 50) {
        uLCD.text_width(2); //4X size text
        uLCD.text_height(2);
        uLCD.color(RED);
        uLCD.locate(0,1);
        uLCD.printf("50 deg"); //Default Green on black text
        uLCD.color(BLUE);
        uLCD.locate(0,0);
        uLCD.printf("40 deg"); //Default Green on black text
        uLCD.locate(0,2);
        uLCD.printf("60 deg"); //Default Green on black text
        uLCD.locate(0,3);
        uLCD.printf("70 deg"); //Default Green on black text
        uLCD.locate(0,4);
        uLCD.printf("80 deg"); //Default Green on black text
      }
      else if (angle_sel == 60) {
        uLCD.text_width(2); //4X size text
        uLCD.text_height(2);
        uLCD.color(RED);
        uLCD.locate(0,2);
        uLCD.printf("60 deg"); //Default Green on black text
        uLCD.color(BLUE);
        uLCD.locate(0,0);
        uLCD.printf("40 deg"); //Default Green on black text
        uLCD.locate(0,1);
        uLCD.printf("50 deg"); //Default Green on black text
        uLCD.locate(0,3);
        uLCD.printf("70 deg"); //Default Green on black text
        uLCD.locate(0,4);
        uLCD.printf("80 deg"); //Default Green on black text
      }
      else if (angle_sel == 70) {
        uLCD.text_width(2); //4X size text
        uLCD.text_height(2);
        uLCD.color(RED);
        uLCD.locate(0,3);
        uLCD.printf("70 deg"); //Default Green on black text
        uLCD.color(BLUE);
        uLCD.locate(0,0);
        uLCD.printf("40 deg"); //Default Green on black text
        uLCD.locate(0,2);
        uLCD.printf("60 deg"); //Default Green on black text
        uLCD.locate(0,1);
        uLCD.printf("50 deg"); //Default Green on black text
        uLCD.locate(0,4);
        uLCD.printf("80 deg"); //Default Green on black text
      }
      else if (angle_sel == 80) {
        uLCD.text_width(2); //4X size text
        uLCD.text_height(2);
        uLCD.color(RED);
        uLCD.locate(0,4);
        uLCD.printf("80 deg"); //Default Green on black text
        uLCD.color(BLUE);
        uLCD.locate(0,0);
        uLCD.printf("40 deg"); //Default Green on black text
        uLCD.locate(0,2);
        uLCD.printf("60 deg"); //Default Green on black text
        uLCD.locate(0,3);
        uLCD.printf("70 deg"); //Default Green on black text
        uLCD.locate(0,1);
        uLCD.printf("50 deg"); //Default Green on black text
      }
      printf("I am working, done_mode0 = %d\r\n", done_mode0);
    }
    
  }
  led1 = 0; // means this mode is over
  return 0;
}


void exe_rise_userb(MQTT::Client<MQTTNetwork, Countdown>* client)
{

  sprintf(need_to_transmit, "%d", angle_sel);
  if (client->isConnected()) {
			printf("is connected\r\n");
	}
	else {
      printf("isn't connected\r\n");
	}
  mqtt_queue.call(&publish_message, client, need_to_transmit, topic_0);
  
}

void stop_gesture_UI_mode(Arguments *in, Reply *out)
{
  done_mode0 = 1;
  printf("done_mode0 = %d\n", done_mode0);
}

void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client, char *need_to_transmit, const char *topic) {
    message_num++;
    MQTT::Message message;
    char buff[100];
    sprintf(buff, "%s\n", need_to_transmit);

    message.qos = MQTT::QOS0;
    message.retained = false;
    message.dup = false;
    message.payload = (void*) buff;
    message.payloadlen = strlen(buff) + 1;
    int rc = client->publish(topic, message);

    printf("rc:  %d\r\n", rc);
    printf("Puslish message: %s\r\n", buff);
}

void angle_detection_mode(void)
{
  int16_t A_XYZ[3] = {0};
  int i;
  double ori_z;
  double tilt_degree;


  // LED
  led2 = 1; // means mode1 starts
  printf("Please keep the board still\r\n");
  led3 = 1;
  ThisThread::sleep_for(5s);
  

  // initialize the angle z
  for (i = 0, ori_z = 0; i < 10; ++i) {
    BSP_ACCELERO_AccGetXYZ(A_XYZ);
    ori_z += A_XYZ[2];
    ThisThread::sleep_for(200ms);
    printf("initializing #%d\r\n", i);
    // blink LED
    led3 = !led3;
  }

  led3 = 0; // means initiaize is done

  ori_z = ori_z / 10.0;

  // initialize the parameter
  done_mode1 = 0;
  
  uLCD.cls();
  uLCD.text_width(2); //4X size text
  uLCD.text_height(2);
  uLCD.color(RED);
  uLCD.locate(0,2);
  uLCD.printf("sel ang:"); //Default Green on black text
  uLCD.locate(0,3);
  uLCD.printf("%5d deg", angle_sel); //Default Green on black text
  uLCD.color(GREEN);
  uLCD.locate(0,5);
  uLCD.printf("curr ang:"); //Default Green on black text


  while (!done_mode1) {
    BSP_ACCELERO_AccGetXYZ(A_XYZ);
    if (A_XYZ[2] > 0 && A_XYZ[2] > ori_z) A_XYZ[2] = ori_z;
	  else if (A_XYZ[2] < 0 && A_XYZ[2] < -ori_z) A_XYZ[2] = -ori_z;
    tilt_degree = acos(A_XYZ[2] / ori_z) * 180 / 3.1415926;
    // show the current tilt_degree on the ULCD
    uLCD.text_width(2); //4X size text
    uLCD.text_height(2);
    uLCD.color(GREEN);
    uLCD.locate(0,6);
    uLCD.printf("%5.1lf deg", tilt_degree); //Default Green on black text
    sprintf(need_to_transmit, "%lf", tilt_degree);
    if (angle_sel < tilt_degree) mqtt_queue.call(&publish_message, p_client, need_to_transmit, topic_1);

    ThisThread::sleep_for(500ms);
    printf("done_mode1 = %d\n", done_mode1);
  }
  led2 = 0; // means this mode is over

}

void qcall_angle_detection_mode(Arguments *in, Reply *out)
{
  func_queue.call(&angle_detection_mode);
}


void stop_angle_detection_mode(Arguments *in, Reply *out) {
  done_mode1 = 1;
  printf("done_mode1 = %d\n", done_mode1);
}