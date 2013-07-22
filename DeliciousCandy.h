/*
  DeliciousCandy.h - Library for Delicious Candy wireless hackerspace sensorium.
  Created by Issac Merkle for KnoxMakers, 2013.
  Released under the GPLv3.
*/
#ifndef DeliciousCandy_h
#define DeliciousCandy_h

#include "Arduino.h"
#include "../JeeLib/RF12.h"
#include "../JeeLib/Ports.h"

//=========================== HARDWARE =================================

#define MAX_NODE_COUNT   4 // dependant on radio library; affects memory usage
#define RF12_FREQ		 RF12_433MHZ

//=========================== DEFAULTS =================================

#define MASTER_NODE_ID	 1
#define DEFAULT_TIMEOUT	 1000 // max time for response packet in millis

//========================== CONSTANTS =================================

#define NO_ACTIVE_NODE   255 // unusable node_id signifying no active node

enum CALLBACK_TYPES {
  CB_STATIC,
  CB_CONTROLLER,
  CB_NODE
};

enum MSG_TYPES {
  ATTACH_NODE = 'a',
  SEND_UPDATE,
  UPDATE_SENSOR,
  UPDATE_COMPLETE,
  POLL_COMPLETE,
  DETACH_NODE,
  STOP_POLLING,
  START_POLLING,
  PURGE_NODES
};

//========================== TYPES =====================================

typedef struct {
  byte node_id;
  enum MSG_TYPES msg_type;
  union {
    struct {
      unsigned long poll_interval;
    } attach_node;
    struct {
      byte sensor_id;
      byte byte_reading;
    } sensor_byte;
  } data;
} Message;

//======================================================================
// forward declarations necessary so CandyNet can contain pointers to
// CandyController and CandyNode member functions
class CandyController;
class CandyNode;
//======================================================================

class CandyNet
{
  public:
    Message* msg_in;
    Message* msg_out;

    CandyNet(byte node_id,CandyController* controller,unsigned long node_timeout);
    CandyNet(byte node_id,CandyNode* node);
    CandyNet(byte node_id,void (*cb_rx)(void),void (*cb_rx_timeout)(void),unsigned long node_timeout);
    CandyNet(byte node_id,void (*cb_rx)(void),void (*cb_rx_timeout)(void));
    void heartbeat();
    void send_msg();
    void send_msg_expectantly();
    boolean busy();
    void reset_rx_timeout();
    void rx_seq_complete();
    
  private:
    enum CALLBACK_TYPES _cb_type;
    boolean _wireless_tx_pending, _wireless_rx_pending;
    unsigned long _node_timeout;
    unsigned long _timeout_at;
    void (*_cb_rx)(void);
    void (*_cb_rx_timeout)(void);
    CandyController* _controller;
    CandyNode* _node;

    void init(byte node_id, unsigned long node_timeout, CALLBACK_TYPES cb);    
    void wireless_rx();
    void wireless_tx();
    void do_rx_callback();
    void do_rx_timeout_callback();
};

//======================================================================

class CandyController
{
  public:
    CandyController(void (*cb_rx)(void),void (*cb_rx_timeout)(void),unsigned long node_timeout);
    CandyController(void (*cb_rx)(void),void (*cb_rx_timeout)(void));
    void heartbeat();
    void catch_rx();
    void catch_timeout();
    void register_node(byte node_id, unsigned long poll_interval);

  private:
    typedef struct {
      byte node_id;
      unsigned long poll_interval;
      unsigned long next_poll;
    } Node;
    Node _nodes[MAX_NODE_COUNT];
    byte _node_count;
    byte _active_node;
    CandyNet *_network;
    void (*_cb_rx)(void);
    void (*_cb_rx_timeout)(void);
    
    byte get_node_idx(byte node_id);
    void schedule_next_poll(byte node_id);
    void check_clock();
    void begin_node_poll(byte node_id);
    void end_node_poll();

};

//======================================================================

class CandyNode
{
  public:
    CandyNode(byte node_id,void (*cb_update_sensors)(void));
    void heartbeat();
    void catch_rx();
    void catch_timeout();
    void send_sensor_byte(byte sensor_id, byte sensor_reading);
    void done_updating();
  private:
    CandyNet *_network;
    void (*_cb_update_sensors)(void);
};

//======================================================================

#endif
