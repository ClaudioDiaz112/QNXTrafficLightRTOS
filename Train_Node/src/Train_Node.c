#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sched.h>
#include "globalmsg.h"
#include <sys/dispatch.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <fcntl.h>

//ENUM Declarations
enum states
{
  State0,
  State1,
  State2,
  State3
};

typedef enum
{
  OFF,
  FLASHING,
  CLOSED,
  OPEN,
  RED,
  GREEN
} Traffic_Light_States;
//OFF=0, FLASHING = 1, CLOSED =2, OPEN=3, RED=4, GREEN=5

// Global Variables/Declarations
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond2 = PTHREAD_COND_INITIALIZER;
sem_t thread_control;
int threadrun = 0;
int data_sent = 1;
int counter = 0;
enum states CurState;

my_data train_message;
train_output train_ouput_states;

// Prototype functions
/////////////////////////////////////////////////////////////////////////////////////////////////////
void control_display(train_output *Light_Output);
void state0_func(train_output *Light_Output);
void state1_func(train_output *Light_Output);
void state2_func(train_output *Light_Output);
void state3_func(train_output *Light_Output);
void state3_func(train_output *Light_Output);
void state_emergency_func(train_output *Light_Output);
void display_output(train_output *Light_Output);
void BoomGateNS_SM(enum states *CurState);
int32_t get_integer_input(void);
error_rc_t receive_control_with_mqueue(int *output);
/////////////////////////////////////////////////////////////////////////////////////////////////////

void control_display(train_output *Light_Output)
{
  train_message.data.train_data.Lights = Light_Output->Lights;
  train_message.data.train_data.BoomGates = Light_Output->BoomGates;
  train_message.data.train_data.States = Light_Output->States;
  train_message.data.train_data.TrainLights = Light_Output->TrainLights;
}

//Functions used to set train_output objects for each state
void state0_func(train_output *Light_Output)
{

  Light_Output->States = 0;
  Light_Output->Lights = FLASHING;
  Light_Output->BoomGates = OPEN;
  Light_Output->TrainLights = GREEN;
}

void state1_func(train_output *Light_Output)
{
  Light_Output->States = 1;
  Light_Output->Lights = FLASHING;
  Light_Output->BoomGates = CLOSED;
  Light_Output->TrainLights = GREEN;
}

void state2_func(train_output *Light_Output)
{
  Light_Output->States = 2;
  Light_Output->Lights = FLASHING;
  Light_Output->BoomGates = OPEN;
  Light_Output->TrainLights = RED;
}

void state3_func(train_output *Light_Output)
{
  Light_Output->States = 3;
  Light_Output->Lights = OFF;
  Light_Output->BoomGates = OPEN;
  Light_Output->TrainLights = RED;
}

void state_emergency_func(train_output *Light_Output)
{
  Light_Output->States = 4;
  Light_Output->Lights = FLASHING;
  Light_Output->BoomGates = CLOSED;
  Light_Output->TrainLights = RED;
}

//function to display status of states
void display_output(train_output *Light_Output)
{
  control_display(&train_ouput_states);
  printf("State = %d\n", Light_Output->States);
  if (Light_Output->Lights == 0)
    printf("LIGHTS = OFF\n");
  else
    printf("LIGHTS = FLASHING\n");
  if (Light_Output->BoomGates == 2)
    printf("BOOMGATES = CLOSED\n");
  else
    printf("BOOMGATES = OPEN\n");
  if (Light_Output->TrainLights == 4)
    printf("TRAINLIGHTS = RED\n");
  else
    printf("TRAINLIGHTS = GREEN\n");
  printf("	\n");
}
// state machine function
void BoomGateNS_SM(enum states *CurState_input)
{
  switch (*CurState_input)
  {
  case State0:
    state0_func(&train_ouput_states); //calls state function to set objects
    *CurState_input = State1;
    sleep(4); //simulates gates beginning to close
    break;
  case State1:
    state1_func(&train_ouput_states);
    sleep(4); //simulates train passing through
    *CurState_input = State2;
    break;

  case State2:
    state2_func(&train_ouput_states);
    *CurState_input = State3;
    sleep(4); //simulates gates opening
    break;
  case State3:
    state3_func(&train_ouput_states);
    sleep(4); //simulates time after gates are open and traffic can move through
    *CurState_input = State0;
    break;
  }
  display_output(&train_ouput_states); //calls display status function
}

int32_t get_integer_input(void)
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

/// BEGINNING OF THREADS//////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
void *thread_PeakHour(void *data)
{
  CurState = 0;
  counter = 0;
  while (1)
  {
    sem_wait(&thread_control); //waiting if switching timing threads
    printf("TRAIN CROSSING NODE IS SET TO PEAK HOUR\n");

    while (threadrun != 1)
    {
      sleep(2); //time between trains during peak hours
      printf("\nTrain coming\n");
      CurState = State0; //making sure state machine starts at state 0 each time a new train comes
      counter = 0;
      int Runtimes = 4;
      while (counter < Runtimes) //loop to run 4 times for all 4 states in SM
      {
        pthread_mutex_lock(&mutex);
        while (data_sent == 0 || data_sent == 2 || threadrun == 3)
        {
          pthread_cond_wait(&cond1, &mutex); //locking this thread if communication fails or is waiting to update central with status
        }
        BoomGateNS_SM(&CurState); // pass address and call state machine function
        counter++;                //increment counter
        data_sent = 0;
        pthread_cond_signal(&cond1); //signaling client thread to run to send status to central
        pthread_mutex_unlock(&mutex);
      }
    }

    printf("ENDING TRAIN CROSSING PEAK HOUR THREAD\n");
    sem_post(&thread_control);
  }
  return 0;
}

void *thread_NotPeakHour(void *data)
{
  CurState = 0;
  counter = 0;
  while (1) //infinitely running non peak hour thread until changes
  {
    sem_wait(&thread_control);
    printf("TRAIN CROSSING NODE IS SET TO OFF PEAK HOUR\n");
    while (threadrun != 2)
    {
      sleep(10); //time between trains during off peak hours
      //printf("NOT PEAK HOUR\n");
      printf("\nTrain coming\n"); //notify a train is coming
      CurState = State0;          //making sure state machine starts at state 0 each time a new train comes
      counter = 0;
      int Runtimes = 4;
      while (counter < Runtimes)
      {
        pthread_mutex_lock(&mutex); //locking this thread if communication fails or is waiting to update central with status
        while (data_sent == 0 || data_sent == 2 || threadrun == 3)
        {
          pthread_cond_wait(&cond1, &mutex);
        }
        BoomGateNS_SM(&CurState); // pass address
        counter++;
        data_sent = 0;
        pthread_cond_signal(&cond1); //signaling client thread to run to send status to central
        pthread_mutex_unlock(&mutex);
      }
    }
    sem_post(&thread_control);
    printf("ENDING TRAIN CROSSING NOT PEAK HOUR THREAD\n");
  }
  return 0;
}

void *Emergency_Thread(train_output *Light_Output)
{ //Emergency thread to switch to when uplink communication with central fails or if forced by central/local
  while (1)
  {
    pthread_mutex_lock(&mutex);
    while ((data_sent == 0 || data_sent == 1) && threadrun != 3)
    {
      pthread_cond_wait(&cond1, &mutex); //locking thread until emergency state is called
    }
    printf("\nEmergency thread Running\n");
    state_emergency_func(&train_ouput_states); // calling emergency state
    display_output(&train_ouput_states);       //displaying state status
    data_sent = 0;
    counter = 0;
    CurState = 0;
    sleep(3);
    pthread_cond_signal(&cond1);
    pthread_mutex_unlock(&mutex);
  }
}

void *Client(void *Train_Outputs)
{
  my_reply reply; // replymsg structure for sending back to client

  train_message.ClientID = 100;  // unique number for this client
  train_message.hdr.type = 0x22; // We would have pre-defined data to stuff here

  int server_coid;
  int Flag_to_Send_Data = 1;

  while (Flag_to_Send_Data)
  {
    if (data_sent == 0) //if state machine runs 1st state then enter this
    {
      //printf("  ---> Trying to connect to server named: %s\n", QNET_ATTACH_POINT); //debugging
      if ((server_coid = name_open(QNET_ATTACH_POINT, 0)) == -1)
      {
        printf("\nERROR, could not connect to server!\n\n"); //if IPC fails with central
        data_sent = 2;
      }
      else
      {
        data_sent = 1;
        if (MsgSend(server_coid, &train_message, sizeof(train_message), &reply, sizeof(reply)) == -1)
        {
          printf("Error data NOT sent to server\n");
          data_sent = 2;
        }
        else
        { // if data is sent to central then switch to peak/off peak thread
          data_sent = 1;
        }
      }
    }
    else if (data_sent == 2) //if emergency thread is triggered
    {
      sleep(1);
    }
    else
    {
      data_sent = 1;
    }
    pthread_cond_signal(&cond1);
  }

  return NULL;
}

void *thread_Receive_Msg(void *data)
{
  int input;
  while (1)
  {
    (void)receive_control_with_mqueue(&input);
    if (input == 0)
    {
      printf("-> PeakHour Running!\n");
      threadrun = 2; // set variable for the condvar
    }
    else if (input == 1)
    {
      printf(" <-- OffPeak Running!\n");
      threadrun = 1; // close it
    }
    else if (input == 2)
    {
      threadrun = 3; // emergency thread.
    }
  }
  return NULL;
}

int main(int argc, char *argv[])
{
  printf("STARTING TRAIN CROSSING NODE\n");

  pthread_t th1, th2, th3, th4, th5;

  sem_init(&thread_control, NULL, 1);
  //create threads then only swap if IPC comm is sent, so peak hour thread is constantly running unless swap
  pthread_create(&th1, NULL, thread_PeakHour, NULL); //default starting position
  pthread_create(&th2, NULL, thread_NotPeakHour, NULL);
  pthread_create(&th3, NULL, Client, NULL);
  pthread_create(&th4, NULL, Emergency_Thread, NULL);
  pthread_create(&th4, NULL, thread_Receive_Msg, NULL);

  int input; //scanf for changing thread times, offfpeak, peak, and emergency
  while (1)
  {
    input = get_integer_input();

    if (input == 1)
    {

      printf(" <-- OffPeak Running!\n");
      threadrun = 1; // set variable for the condvar
    }
    else if (input == 2)
    {
      printf("-> PeakHour Running!\n");
      threadrun = 2; // close it
    }
    else if (input == 3)
    {
      threadrun = 3; // emergency thread.
    }
    else if (input == 4)
    {
      threadrun = 2;
    }
  }

  pthread_join(th1, NULL);
  pthread_join(th2, NULL);
  pthread_join(th3, NULL);
  pthread_join(th4, NULL);
  pthread_join(th5, NULL);

  printf("EXITING TRAIN CROSSING NODE\n");

  return 0;
}

error_rc_t receive_control_with_mqueue(int *output)
{
  *output = 100;
  mqd_t qd;
  //error_rc_t rc;
  char buf[MESSAGESIZE] = {}; // buffer needs to be bigger or same size as message being sent
  struct mq_attr attr;
  const char *MqueueLocation = "/test_queue";
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
    (void)remove("../dev/mqueue/test_queue");
  }
  else
  {
    return DID_NOT_RECONNECT;
  }
  return 0;
}
