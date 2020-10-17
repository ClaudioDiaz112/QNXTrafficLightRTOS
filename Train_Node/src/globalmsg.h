#ifndef GLOBALMSG_H
#define GLOBALMSG_H
#include <sys/iofunc.h>

void *Client(void *data);
void *Server(void *data);

#define BUF_SIZE 300
#define ATTACH_POINT "Central_Node_Target" // This must be the same name that is used for the client.
#define QNET_ATTACH_POINT "/net/Central_Node_Target/dev/name/local/Central_Node_Target"
#define MESSAGESIZE 1000
#define Q_FLAGS O_RDWR | O_CREAT | O_EXCL
#define Q_Mode S_IRUSR | S_IWUSR

typedef enum
{
  CONNECTION_FAILED_CLIENTSIDE,
  DID_NOT_RECONNECT
} error_rc_t;

typedef enum
{
  INTERSECTION_NORTH_TARGET,
  INTERSECTION_SOUTH_TARGET,
  TRAIN_NODE_TARGET,
} node_select_t;

typedef union
{ // This replaced the standard:  union sigval
  union
  {
    _Uint32t sival_int;
    void *sival_ptr; // This has a different size in 32-bit and 64-bit systems
  };
  _Uint32t dummy[4]; // Hence, we need this dummy variable to create space
} _mysigval;

typedef struct _Mypulse
{ // This replaced the standard:  typedef struct _pulse msg_header_t;
  _Uint16t type;
  _Uint16t subtype;
  _Int8t code;
  _Uint8t zero[3]; // Same padding that is used in standard _pulse struct
  _mysigval value;
  _Uint8t zero2[2]; // Extra padding to ensure alignment access.
  _Int32t scoid;
} msg_header_t;

typedef struct //structure for train crossing objects
{
  int Lights;
  int BoomGates;
  int States;
  int TrainLights;
} train_output;

typedef struct //structure for train crossing objects
{
  int state;
  int light_state[12];
  ;

} intersection_output;

typedef union
{
  train_output train_data;
  intersection_output intersection_data;
} data;

typedef struct
{
  msg_header_t hdr; // Custom header
  int ClientID;     // our data (unique id from client)
  data data;        // our data <-- This is what we are here for
} my_data;

typedef struct
{
  msg_header_t hdr;   // Custom header
  char buf[BUF_SIZE]; // Message to send back to send back to other thread
} my_reply;

#endif
