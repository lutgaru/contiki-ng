  /*
 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 */

/**
 * \file
 *      Erbium (Er) CoAP client example.
 * \author
 *      Matthias Kovatsch <kovatsch@inf.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include "net/routing/routing.h"
#include "sys/rtimer.h"
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "testglobal.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"
//#include "tsch-schedule.h"
#include "uip-ds6-nbr.h"
#if PLATFORM_SUPPORTS_BUTTON_HAL
#include "dev/button-hal.h"
#else
#include "dev/button-sensor.h"
#endif

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP
#define IN_HT 2
#define NOT_EXIST 0
#define EXIST 1
#define CONNECTED 3

/* FIXME: This server address is hard-coded for Cooja and link-local for unconnected border router. */
//#define SERVER_EP "coaps://[fd00::212:4b00:a55:dd05]"
//#define SERVER_EP "coaps://[fe80::212:4b00:a55:dd05]"
//#define SERVER_EP "coaps://[fd00::200:0:0:1]"
//#define SERVER_EP "coaps://[fd00::202:2:2:2]"
#define SERVER_EP "coaps://[fe80::200:0:0:1]"
static struct rtimer timer_rtimer;
static int second_counter=0;

extern coap_resource_t
  test_metric;

#define TOGGLE_INTERVAL 0.001
#define NUMBERS_OF_NODES 3

PROCESS(er_example_client, "Erbium Example Client");
AUTOSTART_PROCESSES(&er_example_client);

static struct etimer et;
rtimer_clock_t timerdiff[2];
/* Example URIs that can be queried. */
#define NUMBER_OF_URLS 5
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
char *service_urls[NUMBER_OF_URLS] =
{ "/test/hello", "sensors/temperature", "battery/", "error/in//path", "/test/chunks" };
#if PLATFORM_HAS_BUTTON
static int uri_switch = 0;
#endif

static const char *
ds6_nbr_state_to_str(uint8_t state)
{
  switch(state) {
    case NBR_INCOMPLETE:
      return "Incomplete";
    case NBR_REACHABLE:
      return "Reachable";
    case NBR_STALE:
      return "Stale";
    case NBR_DELAY:
      return "Delay";
    case NBR_PROBE:
      return "Probe";
    default:
      return "Unknown";
  }
}

/* This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses. */
void
client_chunk_handler(coap_message_t *response)
{
  const uint8_t *chunk;

  if(response == NULL) {
    puts("Request timed out");
    return;
  }

  int len = coap_get_payload(response, &chunk);
  printf("%d\n",len);
  //printf("|%.*x\n", len, (char *)chunk);
  for(int x=0;x<len;x++){
    printf(" %x",chunk[x]);
  }
}
PROCESS_THREAD(er_example_client, ev, data)
{
  PROCESS_BEGIN();
  static coap_endpoint_t server_ep[NUMBERS_OF_NODES];
  static uint8_t connect_intent = 0;
  static uint8_t muestras = 0;
  static uint8_t node_conn=0;
  static uint8_t status_node[NUMBERS_OF_NODES];
  static uint8_t server_epp[NUMBERS_OF_NODES][26];
  NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER,-21);
  //memset(status_node,0,NUMBERS_OF_NODES);
  coap_activate_resource(&test_metric, "test/metric");
  NETSTACK_ROUTING.root_start();
  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */
  //coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);
  //coap_endpoint_connect(&server_ep);
  
  printf("inicio:%lu\n",RTIMER_NOW());
  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);

  while(1) {
    PROCESS_YIELD();
    if(etimer_expired(&et)){
      if(uip_sr_num_nodes() > 0) {
        uip_sr_node_t *link;
        uip_ipaddr_t child_ipaddr;
        int node=1;
        int n;
        /* Our routing links */
        link = uip_sr_node_head();
        link = uip_sr_node_next(link);
        while(link != NULL) {
          NETSTACK_ROUTING.get_sr_node_ipaddr(&child_ipaddr, link);
          
          if (status_node[node]==NOT_EXIST){
            n = snprintf(server_epp, 26, "coaps://[fd00::200:%u:%u:%u]",
                  child_ipaddr.u8[13], child_ipaddr.u8[14], child_ipaddr.u8[15]);
            printf("ep: %s, %d\n",server_epp,status_node[node]);
            coap_endpoint_parse(server_epp, strlen(server_epp), &server_ep[node]);
            status_node[node]=EXIST;
          }
          if (!coap_endpoint_is_connected(&server_ep[node]) && status_node[node]==EXIST){
            coap_endpoint_connect(&server_ep[node]);
            status_node[node]=IN_HT;
          }
          else if(coap_endpoint_is_connected(&server_ep[node])&& status_node[node]!=CONNECTED){
            printf("seguro %d: %lu\n",node,RTIMER_NOW());
            status_node[node]=CONNECTED;
          }
          
          node++;
          link = uip_sr_node_next(link);
        }
      }
      etimer_reset(&et);
    }
  }

  PROCESS_END();
}
