#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sched.h>
#include "globalmsg.h"
#include <sys/dispatch.h>
#include <string.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <fcntl.h>

enum states
{
  State0,
  State1,
  State2,
  State3,
  State4,
  State5,
  State6,
  State7,
  State8,
  State9,
  State10,
  State11,
  State12,
  State13,
  Emergency
};
typedef enum
{
  OFF,
  RED,
  YELLOW,
  GREEN,
  FLASHING_RED
} Traffic_Light_States;

enum peak_states
{
  PEAKHOUR,
  OFFPEAK,
  EMERGENCY
};
typedef struct sensor_states
{
  int EW_sensor;
  int NS_ped_sensor;
} sensor_states;
struct Intersection_Light_Outputs
{

  int north_Light;
  int south_Light;
  int east_Light;
  int west_Light;

  int north_Left_Light;
  int south_Left_Light;
  int east_Left_Light;
  int west_Left_Light;

  int north_Ped_Light;
  int south_Ped_Light;
  int east_Ped_Light;
  int west_Ped_Light;

  int current_state;
};

struct Intersection_Light_Outputs Intersection2;
my_data intersection_msg;
pthread_mutex_t messagesending = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condvar = PTHREAD_COND_INITIALIZER;

int datahour;
int boom_state;
int peak_state;
int data_sent = 0;
// Prototype functions
/////////////////////////////////////////////////////////////////////////////////////////////////////

error_rc_t receive_control_with_mqueue(int *output);
char *return_state(int val);
void intersection_SM(enum states *current_State, void *sensors);
void *thread_PeakHour(void *datahour);
void *thread_OffPeak(void *datahour);
void *emergency_thread(void *datahour);
void *command_received(void *datahour);
void *Client(void *data);
void *car_ped_sensors_thread(void *sensordata);
int get_integer_input(void);
void control_display();
void display_output(struct Intersection_Light_Outputs *Light_Output);
void state_function(int current, int N_light, int S_light, int E_light, int W_light, int NL_light,
                    int SL_light, int EL_light, int WL_light, int NP_light, int SP_light, int EP_light, int WP_light,
                    struct Intersection_Light_Outputs *Light_Output);
///////////////////////////////////////////////////////////////////////////////////////////////////////

int main(void)
{
  puts("Project - Intersection"); /* prints Hello World!!! */
  peak_state = 0;
  //flag = 0;

  //create thread
  pthread_t th1, th2, th3, th4, th5;

  pthread_attr_t th1_attr;
  pthread_attr_init(&th1_attr);
  struct sched_param th1_param;
  th1_param.sched_priority = 20;
  pthread_attr_setschedpolicy(&th1_attr, SCHED_RR);
  pthread_attr_setschedparam(&th1_attr, &th1_param);

  pthread_create(&th1, NULL, thread_PeakHour, NULL);
  pthread_create(&th2, NULL, thread_OffPeak, NULL);
  pthread_create(&th3, NULL, Client, NULL);
  pthread_create(&th4, NULL, emergency_thread, NULL);

  pthread_create(&th5, &th1_attr, command_received, NULL);
  pthread_join(th1, NULL);
  pthread_join(th2, NULL);
  pthread_join(th3, NULL);
  pthread_join(th4, NULL);
  pthread_join(th5, NULL);

  /*enum states current_state = State0;
	 intersection_SM(&current_state);*/

  ;

  puts("End of program");
  return EXIT_SUCCESS;
}

/* 	State List:
 state 0 = initial state (all red).
 state 1 = N/S NLeft/SLeft green.
 state 2 = N/S Green. Nleft/SLeft Off. Peds E/W green
 state 3 = N/S Green. E/W Ped flash Red.
 State 4 = N/S Yellow. E/W Ped Hard Red.

 boom gates lowered states:
 state 5 = S SLeft green.
 state 6 = S green Sleft OFF . EW peds green
 state 7 = S Green. E/W Peds flash red
 state 8 = S yellow. E/W Peds RED

 state 9 = All Red
 state 10 = E/W ELeft/WLeft green.
 state 11 = E/W Green. Eleft/WLeft Off. Peds N/S green
 state 12 = E/W Green. N/S Ped flash Red.
 state 13 = E/W Yellow. N/S Ped Hard Red.

 Emergency = all lights blink yellow
 */
void *emergency_thread(void *datahour)
{
  while (1)
  {

    pthread_mutex_lock(&messagesending);
    while ((data_sent == 0 || data_sent == 1) && peak_state != 2) // checks
      pthread_cond_wait(&condvar, &messagesending);

    printf("IN EMERGENCY STATE ");
    state_function(14, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, &Intersection2);
    display_output(&Intersection2);
    data_sent = 0;
    sleep(1);
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&messagesending);
  }
}
void *command_received(void *datahour)
{
  error_rc_t rc;
  int input;
  while (1)
  {
    (void)receive_control_with_mqueue(&input);

    if (input >= 0 && input <= 2)
    {
      peak_state = input;
    }
    else if (input == 4) //boom gates are up safe to pass
    {
      boom_state = 0;
    }
    else if (input == 5) //boom gates closing
    {
      boom_state = 1;
    }
    pthread_cond_wait(&condvar, &messagesending);
  }
}
int get_integer_input(void)
{
  int32_t input;
  char *end;
  char buf[30];
  do
  {
    if (!fgets(buf, sizeof buf, stdin))
    {
      break;
    }
    buf[strlen(buf) - 1] = 0;
    input = strtol(buf, &end, 10);
  } while (end != buf + strlen(buf));
  return input;
}

void *thread_PeakHour(void *datahour)
{
  enum states current_state = State0;
  int sensor;
  while (1)
  {

    pthread_mutex_lock(&messagesending);
    while ((data_sent == 0 || data_sent == 2) || peak_state == 2)
      pthread_cond_wait(&condvar, &messagesending);
    if (peak_state == PEAKHOUR)
      intersection_SM(&current_state, &sensor);
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&messagesending);
  }
  return 0;
}

void *thread_OffPeak(void *datahour)
{
  enum states current_state = State0;
  int sensor = 0;
  pthread_t th6;

  pthread_create(&th6, NULL, car_ped_sensors_thread, &sensor);
  while (1)
  {
    pthread_mutex_lock(&messagesending);
    while ((data_sent == 0 || data_sent == 2) || peak_state == 2)
      pthread_cond_wait(&condvar, &messagesending);
    if (peak_state == OFFPEAK)
      intersection_SM(&current_state, &sensor);
    pthread_cond_signal(&condvar);
    pthread_mutex_unlock(&messagesending);
  }
  return 0;
}
void *car_ped_sensors_thread(void *sensordata)
{
  while (1)
  {
    //printf("input: 0 = reset, 1 = E/W car sensor trigger, 2 = N/S ped sensor, 3 = both sensors triggered");
    int sensor_trigger = get_integer_input();
    if (sensor_trigger == 0)
    {
      //no sensor has been triggered
      *((int *)sensordata) = 0;
    }
    else if (sensor_trigger == 1)
    {
      *((int *)sensordata) = 1;
      //E/W sensor has been triggered
    }
  }
}
void intersection_SM(enum states *current_State, void *sensors)
{

  //int cur_state =
  //int offpeak_active=peakhour_state;

  switch (*current_State)
  {

  case State0:
    //Check if the boom gates are lowered
    //			N, 		S, 		E, 		W,	NLeft, 	Sleft, 	Eleft, Wleft, Npeds, Speds, Epeds, Wpeds
    state_function(0, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, &Intersection2);
    // if gate state is 0 then continue as normal else go to boom gate states.
    if (boom_state == 0)
    {
      *current_State = State1;
    }
    else if (boom_state == 1)
      *current_State = State5;
    break;

  case State1:
    //arguments for function:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
    state_function(1, GREEN, GREEN, RED, RED, GREEN, GREEN, RED, RED, RED, RED, RED, RED, &Intersection2);

    //state1_func(&Intersection2);

    if (peak_state == OFFPEAK && *((int *)sensors) == 0)
    {

      printf("input: 0 = reset, 1 = sensor trigger\n");
      while (peak_state == OFFPEAK && *((int *)sensors) == 0)
        *current_State = State1;
    }
    sleep(1);
    *((int *)sensors) = 0;
    *current_State = State2;

    break;

  case State2:
    //		  state,	N,	S,	 	E,	 W,	 NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
    state_function(2, GREEN, GREEN, RED, RED, OFF, OFF, RED, RED, RED, RED, GREEN, GREEN, &Intersection2); // state2_func(&Intersection2);
    *current_State = State3;
    break;

  case State3:
    //		  state,  N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,		WPed, 		light state structure
    state_function(3, GREEN, GREEN, RED, RED, OFF, OFF, RED, RED, RED, RED, FLASHING_RED, FLASHING_RED, &Intersection2);
    // state3_func(&Intersection2);
    *current_State = State4;
    break;

  case State4:
    //		   state,	N,	  S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
    state_function(4, YELLOW, YELLOW, RED, RED, OFF, OFF, RED, RED, RED, RED, RED, RED, &Intersection2);
    // state4_func(&Intersection2);
    *current_State = State9;
    break;

  case State5:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
	  state_function(5, GREEN, RED, RED, RED, GREEN, RED, RED, RED, RED, RED, RED, RED, &Intersection2);
    if (peak_state == OFFPEAK)
    {
      if (boom_state == 0)
        *current_State = State1;
      else
        *current_State = State5;
    }
    else
      *current_State = State6;
    // state5_func(&Intersection2);
    *current_State = State6;
    break;

  case State6:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
	  state_function(6, GREEN, RED, RED, RED, OFF, RED, RED, RED, GREEN, GREEN, RED, RED, &Intersection2);

    // state6_func(&Intersection2);
    *current_State = State7;
    break;

  case State7:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
	  state_function(7, GREEN, RED, RED, RED, OFF, RED, RED, RED, FLASHING_RED, FLASHING_RED, RED, RED, &Intersection2);

    //state7_func(&Intersection2);
    *current_State = State8;
    break;

  case State8:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
	  state_function(8, YELLOW, RED, RED, RED, OFF, RED, RED, RED, RED, RED, RED, RED, &Intersection2);
    //state8_func(&Intersection2);
    *current_State = State9;
    break;

  case State9:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
    state_function(9, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, RED, &Intersection2);

    // state9_func(&Intersection2);
    *current_State = State10;
    break;

  case State10:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
    state_function(10, RED, RED, GREEN, GREEN, RED, RED, GREEN, GREEN, RED, RED, RED, RED, &Intersection2);

    // state10_func(&Intersection2);
    *current_State = State11;
    break;

  case State11:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
    state_function(11, RED, RED, GREEN, GREEN, RED, RED, OFF, OFF, GREEN, GREEN, RED, RED, &Intersection2);

    //  state11_func(&Intersection2);
    *current_State = State12;
    break;
  case State12:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure
    state_function(12, RED, RED, GREEN, GREEN, RED, RED, OFF, OFF, FLASHING_RED, FLASHING_RED, RED, RED, &Intersection2);

    // state12_func(&Intersection2);
    *current_State = State13;
    break;

  case State13:
    //		   state,	N,	S,	 E,	 W,	NLeft,	SLeft,	ELeft, 	WLeft,	NPed,	SPed,	EPed,	WPed, 	light state structure

    state_function(13, RED, RED, YELLOW, YELLOW, RED, RED, OFF, OFF, RED, RED, RED, RED, &Intersection2);

    //state13_func(&Intersection2);
    *current_State = State0;
    break;
  case Emergency:
    state_function(13, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, YELLOW, &Intersection2);
    break;
  default:
    *current_State = State0;
  }
  display_output(&Intersection2);
  sleep(4);
}

// This function will grab check the signal from control and return a value.
// If no connection is found then it will go to emergency state

// Display all the traffic/pedestrian output lights to the console
void display_output(struct Intersection_Light_Outputs *Light_Output)
{
  control_display(); // refresh display to control
  data_sent = 0;     // ready to send data again
  printf("State %i\n", Light_Output->current_state);
  printf("North state = %s", return_state(Light_Output->north_Light));
  printf("		");
  printf("South State = %s", return_state(Light_Output->south_Light));
  printf("		");
  printf("East State = %s", return_state(Light_Output->east_Light));
  printf("		");
  printf("West State = %s \n", return_state(Light_Output->west_Light));

  printf("North left state = %s ", return_state(Light_Output->north_Left_Light));
  printf("	");
  printf("South left state = %s ", return_state(Light_Output->south_Left_Light));
  printf("	");
  printf("East left state = %s ", return_state(Light_Output->east_Left_Light));
  printf("	");
  printf("West left State = %s \n", return_state(Light_Output->west_Left_Light));

  printf("North Peds State = %s", return_state(Light_Output->north_Ped_Light));
  printf("	");
  printf("South Peds State = %s", return_state(Light_Output->south_Ped_Light));
  printf("	");
  printf("East Peds State = %s", return_state(Light_Output->east_Ped_Light));
  printf("	");
  printf("West Peds State = %s \n\n", return_state(Light_Output->west_Ped_Light));
}
void *Client(void *data)
{
  control_display(); // set display states for sending

  my_reply reply;                   // replymsg structure for sending back to client
  intersection_msg.ClientID = 200;  // unique number for this client
  intersection_msg.hdr.type = 0x22; // We would have pre-defined data to stuff here

  int server_coid;
  int Flag_to_Send_Data = 1;
  while (Flag_to_Send_Data)
  {
    if (data_sent == 0)
    {
      // printf("  ---> Trying to connect to server named: %s\n", QNET_ATTACH_POINT);
      if ((server_coid = name_open(QNET_ATTACH_POINT, 0)) == -1)
      {
           printf("\n    ERROR, could not connect to server!\n\n");
        //Flag_to_Send_Data = 0;
        // cannot_connect = 1;
        //return EXIT_FAILURE;
		data_sent=2;
      }
	  else
	  {

		  data_sent =1;


      // set up data packet

      if (MsgSend(server_coid, &intersection_msg, sizeof(intersection_msg), &reply, sizeof(reply)) == -1)
      {
         printf(" Error data '%d' NOT sent to server\n", intersection_msg.data);
        // maybe we did not get a reply from the server
        //  cannot_connect = 1;
        data_sent = 2;
        //break;
      }
      else
      { // now process the reply
        data_sent = 1;
      }
	  }
      //sleep(5);	// wait a few seconds before sending the next data packet
    }
    else if (data_sent == 2)
    {
      // printf("retrying to connect\n");
      sleep(1);
    }
    else
    {
      data_sent = 1;
    }
    pthread_cond_signal(&condvar);
  }
 // sched_yield();
  return NULL;
}
void control_display()
{
  intersection_msg.data.intersection_data.light_state[0] = Intersection2.north_Light;
  intersection_msg.data.intersection_data.light_state[1] = Intersection2.south_Light;
  intersection_msg.data.intersection_data.light_state[2] = Intersection2.east_Light;
  intersection_msg.data.intersection_data.light_state[3] = Intersection2.west_Light;
  intersection_msg.data.intersection_data.light_state[4] = Intersection2.north_Left_Light;
  intersection_msg.data.intersection_data.light_state[5] = Intersection2.south_Left_Light;
  intersection_msg.data.intersection_data.light_state[6] = Intersection2.east_Left_Light;
  intersection_msg.data.intersection_data.light_state[7] = Intersection2.west_Left_Light;
  intersection_msg.data.intersection_data.light_state[8] = Intersection2.north_Ped_Light;
  intersection_msg.data.intersection_data.light_state[9] = Intersection2.south_Ped_Light;
  intersection_msg.data.intersection_data.light_state[10] = Intersection2.east_Ped_Light;
  intersection_msg.data.intersection_data.light_state[11] = Intersection2.west_Ped_Light;
  intersection_msg.data.intersection_data.state = Intersection2.current_state;
}
char *return_state(int val)
{

  if (val == 0)
    return "OFF (0)";
  else if (val == 1)
    return "RED (1)";
  else if (val == 2)
    return "YELLOW(2)";
  else if (val == 3)
    return "GREEN (3)";
  else if (val == 4)
    return "FLASH RED(4)";
  else
    return "ERROR";
}

void state_function(int current, int N_light, int S_light, int E_light, int W_light, int NL_light,
                    int SL_light, int EL_light, int WL_light, int NP_light, int SP_light, int EP_light, int WP_light,
                    struct Intersection_Light_Outputs *Light_Output)
{

  Light_Output->current_state = current;
  Light_Output->north_Light = N_light;
  Light_Output->south_Light = S_light;
  Light_Output->east_Light = E_light;
  Light_Output->west_Light = W_light;

  Light_Output->north_Left_Light = NL_light;
  Light_Output->south_Left_Light = SL_light;
  Light_Output->east_Left_Light = EL_light;
  Light_Output->west_Left_Light = WL_light;

  Light_Output->north_Ped_Light = NP_light;
  Light_Output->south_Ped_Light = SP_light;
  Light_Output->east_Ped_Light = EP_light;
  Light_Output->west_Ped_Light = WP_light;
}
//set light outputs to the corresponding state
error_rc_t receive_control_with_mqueue(int *output)
{
  *output = 100;
  mqd_t qd;
  error_rc_t rc;
  int ret;
  char buf[MESSAGESIZE] = {}; // buffer needs to be bigger or same size as message being sent
  struct mq_attr attr;

  // // example using the default path notation.
  const char *MqueueLocation = "/test_queue";
  //const char * MqueueLocation = "/net/VM_x86_Target02/test_queue";

  qd = mq_open(MqueueLocation, O_RDONLY); //MqueueLocation should be opened on the node where the queue was established
  if (qd != -1)
  {
    mq_getattr(qd, &attr);
    // printf("max. %ld msgs, %ld bytes; waiting: %ld\n", attr.mq_maxmsg, attr.mq_msgsize, attr.mq_curmsgs);

    while (mq_receive(qd, buf, attr.mq_msgsize, NULL) > 0) //wait for the messages
    {
      //  printf("dequeue: '%s'\n", buf); //print out the messages to this terminal
      if (strcmp(buf, "done") != 0)
      {
        *output = atoi(buf);
      }
      if (!strcmp(buf, "done")) //once we get this message we know not to expect any more mqueue data
        break;
    }
    mq_close(qd);
    ret = remove("../dev/mqueue/test_queue");
  }
  else
  {
    rc = DID_NOT_RECONNECT;
  }
  return 0;
}
