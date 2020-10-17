#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sched.h>
#include "globalmsg.h"
#include <sys/dispatch.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <fcntl.h>
// This Node runs a server that gets packets of data, determines where it came from and displays accordingly.
name_attach_t *attach;

void *server(void *data);
error_rc_t send_control_with_mqueue(char data, node_select_t node_location);
char *return_state(int val);

int main(void)
{
  pthread_t th1;
  pthread_create(&th1, NULL, server, NULL);
  pthread_join(th1, NULL);
  return 0;
}
// Server that takes all packets from local nodes.
void *server(void *data)
{
  my_data msg;
  my_reply replymsg;           // replymsg structure for sending back to client
  replymsg.hdr.type = 0x01;    // some number to help client process reply msg
  replymsg.hdr.subtype = 0x00; // some number to help client process reply msg
  // Create a global name (/dev/name/local/...)
  if ((attach = name_attach(NULL, ATTACH_POINT, 0)) == NULL)
  // Create a global name (/dev/name/global/...)
  //if ((attach = name_attach(NULL, ATTACH_POINT, NAME_FLAG_ATTACH_GLOBAL)) == NULL)
  {
    printf("\nFailed to name_attach on ATTACH_POINT: %s \n", ATTACH_POINT);
    printf("\n Possibly another server with the same name is already running or you need to start the gns service!\n");
    return EXIT_FAILURE;
  }
  printf("Server Listening for Clients on ATTACH_POINT: %s \n", ATTACH_POINT);

  int rcvid = 0, msgnum = 0;      // no message received yet
  int Stay_alive = 1, living = 0; // server stays running (ignores _PULSE_CODE_DISCONNECT request)
  living = 1;
  while (living)
  {

    // Do your MsgReceive's here now with the chid
    rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);

    if (rcvid == -1) // Error condition, exit
    {
      printf("\nFailed to MsgReceive\n");
      break;
    }

    // did we receive a Pulse or message?
    // for Pulses:
    if (rcvid == 0) //  Pulse received, work out what type
    {

      printf("\nServer Disconnected from ClientID:%d :\n", msg.ClientID);
      //printf("Pulse received:%d \n", msg.hdr.code);

      switch (msg.hdr.code)
      {
      case _PULSE_CODE_DISCONNECT:

        if (Stay_alive == 0)
        {
          ConnectDetach(msg.hdr.scoid);
          if (msg.ClientID == 100 || msg.ClientID == 300)
          {
            // send_control_with_mqueue(2, INTERSECTION_NORTH_TARGET);
            //send_control_with_mqueue(2, INTERSECTION_SOUTH_TARGET);
          }
          printf("\nServer Disconnected from ClientID:%d :\n", msg.ClientID);
          living = 0; // kill while loop
          continue;
        }
        else
        {
          if (msg.ClientID == 100)
          {
            // send_control_with_mqueue(2, INTERSECTION_NORTH_TARGET);
            // send_control_with_mqueue(2, INTERSECTION_SOUTH_TARGET);
          }
        }
        break;

      case _PULSE_CODE_UNBLOCK:
        // REPLY blocked client wants to unblock (was hit by a signal
        // or timed out).  It's up to you if you reply now or later.
        printf("\nServer got _PULSE_CODE_UNBLOCK after %d, msgnum\n", msgnum);
        break;

      case _PULSE_CODE_COIDDEATH: // from the kernel
        printf("\nServer got _PULSE_CODE_COIDDEATH after %d, msgnum\n", msgnum);
        break;

      case _PULSE_CODE_THREADDEATH: // from the kernel
        printf("\nServer got _PULSE_CODE_THREADDEATH after %d, msgnum\n", msgnum);
        break;

      default:
        // Some other pulse sent by one of your processes or the kernel
        printf("\nServer got some other pulse after %d, msgnum\n", msgnum);
        break;
      }
      continue; // go back to top of while loop
    }

    // for messages:
    if (rcvid > 0) // if true then A message was received
    {
      MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
      if (msg.ClientID == 200)
      {
        printf("-------------------------Intersection Node North --------------------------\n");
        printf("State %i\n", msg.data.intersection_data.state);
        printf("North state = %s", return_state(msg.data.intersection_data.light_state[0]));
        printf("        ");
        printf("South State = %s", return_state(msg.data.intersection_data.light_state[1]));
        printf("        ");
        printf("East State = %s", return_state(msg.data.intersection_data.light_state[2]));
        printf("        ");
        printf("West State = %s \n", return_state(msg.data.intersection_data.light_state[3]));

        printf("North left state = %s ", return_state(msg.data.intersection_data.light_state[4]));
        printf("    ");
        printf("South left state = %s ", return_state(msg.data.intersection_data.light_state[5]));
        printf("    ");
        printf("East left state = %s ", return_state(msg.data.intersection_data.light_state[6]));
        printf("    ");
        printf("West left State = %s \n", return_state(msg.data.intersection_data.light_state[7]));

        printf("North Peds State = %s", return_state(msg.data.intersection_data.light_state[8]));
        printf("    ");
        printf("South Peds State = %s", return_state(msg.data.intersection_data.light_state[9]));
        printf("    ");
        printf("East Peds State = %s", return_state(msg.data.intersection_data.light_state[10]));
        printf("    ");
        printf("West Peds State = %s \n\n", return_state(msg.data.intersection_data.light_state[11]));
        fflush(stdout);
      }
      else if (msg.ClientID == 100)
      {
        printf("-------------------------Train Node  --------------------------\n");
        if (msg.data.train_data.States != 4)
        {
          printf("\n |The current State of train %d| ", msg.data.train_data.States);
          if (msg.data.train_data.Lights == 0)
          {
            printf(" |Boom Gate Lights OFF| ");
          }
          else
          {
            printf(" |Boom Gate Lights Flashing Red| ");
          }

          if (msg.data.train_data.BoomGates == 2)
          {
            printf(" |Boom Gate Closed| ");
          }
          else
          {
            printf(" |Boom Gate Open| ");
          }

          if (msg.data.train_data.TrainLights == 4)
          {
            printf(" |Train Light Red| \n");
          }
          else
          {
            printf("|Train Light Green|\n");
          }
        }
        else
        {
          printf("|Train is in EMERGENCY state|\n");
        }

        fflush(stdout);
        if (msg.data.train_data.Lights == 0)
        {
          send_control_with_mqueue(4, INTERSECTION_NORTH_TARGET);
          send_control_with_mqueue(4, INTERSECTION_SOUTH_TARGET);
        }
        else if (msg.data.train_data.Lights == 1)
        {
          send_control_with_mqueue(5, INTERSECTION_NORTH_TARGET);
          send_control_with_mqueue(5, INTERSECTION_SOUTH_TARGET);
        }
      }

      else if (msg.ClientID == 300)
      {
        printf("-------------------------Intersection Node South --------------------------\n");
        printf("State %i\n", msg.data.intersection_data.state);
        printf("North state = %s", return_state(msg.data.intersection_data.light_state[0]));
        printf("        ");
        printf("South State = %s", return_state(msg.data.intersection_data.light_state[1]));
        printf("        ");
        printf("East State = %s", return_state(msg.data.intersection_data.light_state[2]));
        printf("        ");
        printf("West State = %s \n", return_state(msg.data.intersection_data.light_state[3]));

        printf("North left state = %s ", return_state(msg.data.intersection_data.light_state[4]));
        printf("    ");
        printf("South left state = %s ", return_state(msg.data.intersection_data.light_state[5]));
        printf("    ");
        printf("East left state = %s ", return_state(msg.data.intersection_data.light_state[6]));
        printf("    ");
        printf("West left State = %s \n", return_state(msg.data.intersection_data.light_state[7]));

        printf("North Peds State = %s", return_state(msg.data.intersection_data.light_state[8]));
        printf("    ");
        printf("South Peds State = %s", return_state(msg.data.intersection_data.light_state[9]));
        printf("    ");
        printf("East Peds State = %s", return_state(msg.data.intersection_data.light_state[10]));
        printf("    ");
        printf("West Peds State = %s \n\n", return_state(msg.data.intersection_data.light_state[11]));
        fflush(stdout);
      }
    }
    else
    {
      printf("\nERROR: Server received something, but could not handle it correctly\n");
    }
  }

  // Remove the attach point name from the file system (i.e. /dev/name/local/<myname>)
  name_detach(attach, 0);

  return EXIT_SUCCESS;
  return NULL;
}
//Mqueue to send commands to intersections.
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
                                          // printf("queue: '%s'\n", message);     //print the message to this processes terminal
    mq_send(qd, message, MESSAGESIZE, 0); //send the mqueue
    sprintf(message, "done");
    mq_send(qd, message, MESSAGESIZE, 0); //send the mqueue
                                          // printf("queue: '%s'\n", message);
                                          // printf("Messages sent to the queue\n");
    mq_close(qd);
    mq_unlink(mqueueLocation);
  }
  else
  {
    printf("\nThere was an ERROR opening the message queue!");
  }

  //  printf("\nmqueue send process Exited\n");
  return EXIT_SUCCESS;
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
