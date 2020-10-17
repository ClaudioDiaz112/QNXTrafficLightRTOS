#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <inttypes.h>
#include <string.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "globalmsg.h"

//*Global Variables *//
const int peak_set = 0;
const int offpeak_set = 1;
const int emergency = 2;

//* Function Prototypes*//
void *send(void *data);
error_rc_t send_control_with_mqueue(char data, node_select_t node_location);
error_rc_t receive_control_with_mqueue(int *output, node_select_t node_location);
int32_t get_integer_input(void);
int main(void)
{
  pthread_t th1;
  pthread_create(&th1, NULL, send, NULL);
  pthread_join(th1, NULL);

  return 0;
}
// interface to run commands to the other controllers.
void *send(void *data)
{
  int keyboard_input;
  while (1)
  {
    printf("\nEnter 1 for Train Node Commands\n");
    printf("\nEnter 2 for Intersection North Commands\n");
    printf("\nEnter 3 for  Intersection South Commands\n");
    printf("\nEnter 5 for Peak Hour or Off peak hour control\n\n");
    keyboard_input = get_integer_input();
    if (keyboard_input == 1)
    {

      printf("\nPress 1 for Emergency\n");
      keyboard_input = get_integer_input();

      if (keyboard_input == 1)
      {
        (void)send_control_with_mqueue((emergency), TRAIN_NODE_TARGET);
      }
      else
      {
        printf("\nIncorrect input going to start\n");
      }
    }
    else if (keyboard_input == 2)
    {
      printf("\nPress 1 to set Emergency\n");
      keyboard_input = get_integer_input();
      if (keyboard_input == 1)
      {
        (void)send_control_with_mqueue(emergency, INTERSECTION_NORTH_TARGET);
      }
      else
      {
        printf("\nIncorrect input going to start\n");
      }
    }
    else if (keyboard_input == 3)
    {
      printf("\nPress 1 to set Emergency\n");
      keyboard_input = get_integer_input();
      if (keyboard_input == 1)
      {
        (void)send_control_with_mqueue(emergency, INTERSECTION_SOUTH_TARGET);
      }
      else
      {
        printf("\nIncorrect input going to start\n");
      }
    }
    else if (keyboard_input == 5)
    {
      printf("\nPress 1 to set Peak-Hour\n");
      printf("\nPress 2 to set Off Peak\n");
      keyboard_input = get_integer_input();
      if (keyboard_input == 1)
      {
        (void)send_control_with_mqueue(peak_set, TRAIN_NODE_TARGET);
        (void)send_control_with_mqueue(peak_set, INTERSECTION_NORTH_TARGET);
        (void)send_control_with_mqueue(peak_set, INTERSECTION_SOUTH_TARGET);
      }
      else if (keyboard_input == 2)
      {
        (void)send_control_with_mqueue(offpeak_set, TRAIN_NODE_TARGET);
        (void)send_control_with_mqueue(offpeak_set, INTERSECTION_NORTH_TARGET);
        (void)send_control_with_mqueue(offpeak_set, INTERSECTION_SOUTH_TARGET);
      }
      else
      {
        printf("\nIncorrect input going to start\n");
      }
    }
  }
}
// allows only 0-9 on keyboard as input
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
// allows mqueue to send commands to local controller.
error_rc_t send_control_with_mqueue(char data, node_select_t node_location)
{

  char *mqueueLocation;
  if (node_location == TRAIN_NODE_TARGET)
  {
    mqueueLocation = "/net/Train_Node_Target/test_queue";
  }
  else if (node_location == INTERSECTION_NORTH_TARGET)
  {
    mqueueLocation = "/net/Intersection_North/test_queue";
  }
  else if (node_location == INTERSECTION_SOUTH_TARGET)
  {
    mqueueLocation = "/net/Intersection_South_Target/test_queue";
  }

  mq_unlink(mqueueLocation);
  mqd_t qd;
  char message[MESSAGESIZE] = {};
  struct mq_attr attr;
  attr.mq_maxmsg = 100;
  attr.mq_msgsize = MESSAGESIZE;
  attr.mq_flags = 0;
  attr.mq_curmsgs = 0;
  attr.mq_sendwait = 0;
  attr.mq_recvwait = 0;
  struct mq_attr *my_attr = &attr;
  qd = mq_open(mqueueLocation, Q_FLAGS, Q_Mode, my_attr);
  if (qd != -1)
  {

    sprintf(message, "%d", data);         //put the message in a char[] so it can be sent
    printf("queue: '%s'\n", message);     //print the message to this processes terminal
    mq_send(qd, message, MESSAGESIZE, 0); //send the mqueue
    sprintf(message, "done");
    mq_send(qd, message, MESSAGESIZE, 0); //send the mqueue
    printf("queue: '%s'\n", message);
    printf("Messages sent to the queue\n");
    // as soon as this code executes the mqueue data will be deleted
    // from the /dev/mqueue/test_queue  file structure
    mq_close(qd);

    mq_unlink(mqueueLocation);
  }
  else
  {
    printf("\nThere was an ERROR opening the message queue!");
  }

  printf("\nmqueue send process Exited\n");
  return EXIT_SUCCESS;
}
