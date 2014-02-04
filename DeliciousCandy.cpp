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
  void (*cb_debug)(char*),
  unsigned long node_timeout
){
  //msg_in = (Message*)malloc(sizeof(Message));
  //msg_out = (Message*)malloc(sizeof(Message));
  _cb_rx = cb_rx;
  _cb_rx_timeout = cb_rx_timeout;
  _cb_debug = cb_debug;
  _node_timeout = node_timeout;
  _wireless_tx_pending = false;
  _wireless_rx_pending = false;	
  _timeout_at = 0;

  _radio = RFM12B();
  _radio.Initialize(_node_id,RF12_FREQ);

}

//--------------------------------------------------------------------//

void CandyNet::debug(char* msg){
  _cb_debug(msg);
}

//--------------------------------------------------------------------//

void CandyNet::do_rx_callback(){
  _cb_rx();
}

//--------------------------------------------------------------------//

void CandyNet::do_rx_timeout_callback(){
  _cb_rx_timeout();
}

//--------------------------------------------------------------------//

boolean CandyNet::busy(){
  return (_wireless_tx_pending || _wireless_rx_pending);
}

//--------------------------------------------------------------------//

void CandyNet::wireless_rx(){
	
  if (_radio.ReceiveComplete() && _radio.CRCPass()) {
    memcpy(&msg_in, (byte*) rf12_data, rf12_len);
    do_rx_callback();
  }
  
}

//--------------------------------------------------------------------//

void CandyNet::wireless_tx(){
  if (_wireless_tx_pending && _radio.CanSend()) {
    _radio.SendStart( msg_out.node_id,
                     &msg_out,
                      sizeof(msg_out),
                      false, // no ACKing for now
                      false, // no ACKing for now
                      1); // should invoke SLEEP_MODE_IDLE; consider more aggressive measures for battery power 
    _wireless_tx_pending = false;
  }
}

//--------------------------------------------------------------------//

void CandyNet::reset_rx_timeout(){
  _timeout_at = millis() + _node_timeout;
}

//--------------------------------------------------------------------//

void CandyNet::poll(){
  unsigned long now = millis();
  static unsigned long run_at = 0;
  // see if we've been waiting too long for a response from node
  if (_wireless_rx_pending && (_timeout_at <= now) ){
	debug("CN: rx timeout");
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
  debug("CN: rx seq done");
  //_wireless_tx_pending = false;
}

////////////////////////////////////////////////////////////////////////

CandyController::CandyController(
  void (*cb_rx)(void),
  void (*cb_rx_timeout)(void),
  void (*cb_debug)(char*),
  unsigned long node_timeout
): CandyNet(MASTER_NODE_ID, cb_rx, cb_rx_timeout, cb_debug, node_timeout){
  _node_count = 0;
  _active_node = NO_ACTIVE_NODE;
};

//--------------------------------------------------------------------//

void CandyController::schedule_next_poll(byte node_id){
  byte node_idx = get_node_idx(node_id);
  _nodes[node_idx].next_poll = millis() + _nodes[node_idx].poll_interval;
  debug("CC: poll scheduled");
}

//--------------------------------------------------------------------//

void CandyController::heartbeat(){
  check_clock();
  poll();
}

//--------------------------------------------------------------------//

void CandyController::check_clock(){
  // if idle, set up next node poll, if one is due
  if ( !busy() ){
    for (byte node_idx=0; node_idx<_node_count; node_idx++) {
      if ( _nodes[node_idx].next_poll <= millis() ){
		debug("CC: node time!");
        begin_node_poll(_nodes[node_idx].node_id);
        break;
      };
    }; 
  }
}

//--------------------------------------------------------------------//

void CandyController::begin_node_poll(byte node_id){
  _active_node = node_id;
  msg_out.node_id = _active_node;
  msg_out.msg_type = SEND_UPDATE;
  send_msg_expectantly();
  debug("CC: begin poll");
};

//--------------------------------------------------------------------//

void CandyController::end_node_poll(){
  schedule_next_poll(_active_node);
  _active_node = NO_ACTIVE_NODE;
  debug("CC: end poll");
  rx_seq_complete();
}

//--------------------------------------------------------------------//

void CandyController::do_rx_callback(){
  switch (msg_in.msg_type) {
    case UPDATE_SENSOR:
      reset_rx_timeout();
      _cb_rx();
      break;
    case UPDATE_COMPLETE:
      end_node_poll();
      break;
  }
  _cb_rx();
}

//--------------------------------------------------------------------//

void CandyController::do_rx_timeout_callback(){
  debug("CC: catch timeout");
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
  debug("CC: node registered");
  schedule_next_poll(node_id);
  _node_count += 1;
}

////////////////////////////////////////////////////////////////////////
