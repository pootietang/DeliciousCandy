/*
  DeliciousCandy.cpp - Library for Delicious Candy wireless hackerspace sensorium.
  Created by Issac Merkle for KnoxMakers, 2013.
  Released under the GPLv3.
*/
#include "DeliciousCandy.h"

////////////////////////////////////////////////////////////////////////

CandyNet::CandyNet(
  byte node_id,
  void (*cb_rx)(void),
  void (*cb_rx_timeout)(void),
  unsigned long node_timeout
){
  _cb_rx = cb_rx;
  _cb_rx_timeout = cb_rx_timeout;
  init(node_id,node_timeout,CB_STATIC);
}

CandyNet::CandyNet(
  byte node_id,
  CandyController* controller,
  unsigned long node_timeout
){
  _controller = controller;
  init(node_id,node_timeout,CB_CONTROLLER);
}

CandyNet::CandyNet(
  byte node_id,
  CandyNode* node
){
  _node = node;
  init(node_id,DEFAULT_TIMEOUT,CB_NODE);
}

CandyNet::CandyNet(
  byte node_id,
  void (*cb_rx)(void),
  void (*cb_rx_timeout)(void)
){
  CandyNet(node_id,cb_rx,cb_rx_timeout,DEFAULT_TIMEOUT);
}

//--------------------------------------------------------------------//

void CandyNet::init(byte node_id, unsigned long node_timeout, CALLBACK_TYPES cb){
  // DEBUG - need to allocate memory for msg_in and msg_out here..?
  msg_in = (Message*)malloc(sizeof(Message));
  msg_out = (Message*)malloc(sizeof(Message));
  // msg_in = new Message;
  // msg_out = new Message;
  _wireless_tx_pending = false;
  _wireless_rx_pending = false;	
  _timeout_at = 0;
  _node_timeout = node_timeout;
  _cb_type = cb;
  rf12_initialize(node_id,RF12_FREQ);
}

//--------------------------------------------------------------------//

void CandyNet::do_rx_callback(){
  switch (_cb_type){
    case CB_STATIC:
      _cb_rx();
      break;
    case CB_CONTROLLER:
      _controller->catch_rx();
      break;
    case CB_NODE:
      _node->catch_rx();
      break;
  }
}

//--------------------------------------------------------------------//

void CandyNet::do_rx_timeout_callback(){
  switch (_cb_type){
    case CB_STATIC:
      _cb_rx_timeout();
      break;
    case CB_CONTROLLER:
      _controller->catch_timeout();
      break;
    case CB_NODE:
      _node->catch_timeout();
      break;
  }
}

//--------------------------------------------------------------------//

boolean CandyNet::busy(){
  return (_wireless_tx_pending || _wireless_rx_pending);
}

//--------------------------------------------------------------------//

void CandyNet::wireless_rx(){
  if (rf12_recvDone() && rf12_crc == 0) {
    memcpy(&msg_in, (byte*) rf12_data, rf12_len);
    do_rx_callback();
    rf12_recvDone();
  }
}

//--------------------------------------------------------------------//

void CandyNet::wireless_tx(){
  if (_wireless_tx_pending && rf12_canSend()) {
    rf12_sendStart(msg_out->node_id, &msg_out, sizeof(msg_out));
    // wait for send to finish in "NORMAL" mode (eg no power down)
    rf12_sendWait(0);
    _wireless_tx_pending = false;
  } 
}

//--------------------------------------------------------------------//

void CandyNet::reset_rx_timeout(){
  _timeout_at = millis() + _node_timeout;
}

//--------------------------------------------------------------------//

void CandyNet::heartbeat(){
  unsigned long now = millis();
  static unsigned long run_at = 0;
  // see if we've been waiting too long for a response from node
  if ((_wireless_rx_pending) && (_timeout_at <= now) ){
    do_rx_timeout_callback();
    rx_seq_complete();
  }
  // operate RFM12 no more frequently than 10 times / sec
  if (run_at <= now){
    wireless_rx();
    wireless_tx();
    run_at = millis() + 100;
  }
}

//--------------------------------------------------------------------//

void CandyNet::send_msg(){
  _wireless_tx_pending = true;
}

//--------------------------------------------------------------------//

void CandyNet::send_msg_expectantly(){
  _wireless_rx_pending = true;
  reset_rx_timeout();
  send_msg();
}

//--------------------------------------------------------------------//

void CandyNet::rx_seq_complete(){
  _wireless_rx_pending = false;
}

////////////////////////////////////////////////////////////////////////

CandyController::CandyController(
  void (*cb_rx)(void),
  void (*cb_rx_timeout)(void),
  unsigned long node_timeout
){
  _node_count = 0;
  _active_node = NO_ACTIVE_NODE;
  _cb_rx = cb_rx;
  _cb_rx_timeout = cb_rx_timeout;
  _network = &CandyNet( MASTER_NODE_ID,
                        this,
                        node_timeout );
}

CandyController::CandyController(
  void (*cb_rx)(void),
  void (*cb_rx_timeout)(void)
){
  CandyController( cb_rx,
                   cb_rx_timeout,
                   DEFAULT_TIMEOUT );
}

//--------------------------------------------------------------------//

void CandyController::schedule_next_poll(byte node_id){
  byte node_idx = get_node_idx(node_id);
  _nodes[node_idx].next_poll = millis() + _nodes[node_idx].poll_interval;
}

//--------------------------------------------------------------------//

void CandyController::heartbeat(){
  check_clock();
  (*_network).heartbeat();
}

//--------------------------------------------------------------------//

void CandyController::check_clock(){
  
  // if idle, set up next node poll, if one is due
  if ( !(*_network).busy() ){
    for (byte node_idx=0; node_idx<_node_count; node_idx++) {
      if ( _nodes[node_idx].next_poll <= millis() ){
        begin_node_poll(_nodes[node_idx].node_id);
        break;
      };
    }; 
  }
  
}

//--------------------------------------------------------------------//

void CandyController::begin_node_poll(byte node_id){
  _active_node = node_id;
//  DEBUG -- these lines cause reset
//  is this because msg_in and msg_out are actually unallocated?
  (*_network).msg_out->node_id = _active_node;
  (*_network).msg_out->msg_type = SEND_UPDATE;
  (*_network).send_msg_expectantly();
};

//--------------------------------------------------------------------//

void CandyController::end_node_poll(){
  schedule_next_poll(_active_node);
  _active_node = NO_ACTIVE_NODE;
  (*_network).rx_seq_complete();
}

//--------------------------------------------------------------------//

void CandyController::catch_rx(){
  switch ((*_network).msg_in->msg_type) {
    case UPDATE_SENSOR:
      (*_network).reset_rx_timeout();
      _cb_rx();
      break;
    case UPDATE_COMPLETE:
      end_node_poll();
      break;
  }
  _cb_rx();
}

//--------------------------------------------------------------------//

void CandyController::catch_timeout(){
  end_node_poll();
  _cb_rx_timeout();
}

//--------------------------------------------------------------------//

byte CandyController::get_node_idx(byte node_id){
  for (byte node_idx=0; node_idx<_node_count; node_idx++) {
    if ( _nodes[node_idx].node_id == node_id ){
        return node_idx;
      };
    }; 
}

//--------------------------------------------------------------------//

void CandyController::register_node(
  byte node_id,
  unsigned long poll_interval
){
  byte node_idx = _node_count;
  _nodes[node_idx].node_id = node_id;
  _nodes[node_idx].poll_interval = poll_interval;
  schedule_next_poll(node_id);
  _node_count += 1;
}

////////////////////////////////////////////////////////////////////////

CandyNode::CandyNode(
  byte node_id,
  void (*cb_update_sensors)(void)
){
  _network = &CandyNet(node_id, this);
  (*_network).msg_out->node_id = node_id;
  _cb_update_sensors = cb_update_sensors;
}

//--------------------------------------------------------------------//

void CandyNode::heartbeat(){
  (*_network).heartbeat();
}

//--------------------------------------------------------------------//

void CandyNode::send_sensor_byte(byte sensor_id, byte sensor_reading){
  (*_network).msg_out->msg_type = UPDATE_SENSOR;
  (*_network).msg_out->data.sensor_byte.sensor_id = sensor_id;
  (*_network).msg_out->data.sensor_byte.byte_reading = sensor_reading;
  (*_network).send_msg();	
}

//--------------------------------------------------------------------//

void CandyNode::done_updating(){
  (*_network).msg_out->msg_type = UPDATE_COMPLETE;
  (*_network).send_msg();
}

//--------------------------------------------------------------------//

void CandyNode::catch_rx(){
  switch ((*_network).msg_in->msg_type) {
    case SEND_UPDATE:
      _cb_update_sensors();
      break;
  }
}

//--------------------------------------------------------------------//

void CandyNode::catch_timeout(){
	
}

////////////////////////////////////////////////////////////////////////
