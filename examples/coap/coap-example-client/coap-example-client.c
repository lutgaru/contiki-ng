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

/* FIXME: This server address is hard-coded for Cooja and link-local for unconnected border router. */
#define SERVER_EP "coaps://[fd00::212:4b00:a55:dd05]"
//#define SERVER_EP "coaps://[fd00::200:0:0:1]"
//#define SERVER_EP "coaps://[fd00::203:3:3:3]"
//#define SERVER_EP "coaps://[fd00::200:0:0:1]"
static struct rtimer timer_rtimer;
static int second_counter=0;

extern coap_resource_t
  test_metric;

#define TOGGLE_INTERVAL 0.001

PROCESS(er_example_client, "Erbium Example Client");
AUTOSTART_PROCESSES(&er_example_client);

static struct etimer et;
rtimer_clock_t timerdiff[2];
/* Example URIs that can be queried. */
#define NUMBER_OF_URLS 4
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
char *service_urls[NUMBER_OF_URLS] =
{ "/test/hello", "/actuators/toggle", "battery/", "error/in//path" };
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

  printf("|%.*s", len, (char *)chunk);
}
PROCESS_THREAD(er_example_client, ev, data)
{
  PROCESS_BEGIN();
  static coap_endpoint_t server_ep;
  static uint8_t connect_intent = 0;
  coap_activate_resource(&test_metric, "test/metric");
  NETSTACK_ROUTING.root_start();
  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */

  //NETSTACK_ROUTING.root_start();
  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);
  //coap_endpoint_connect(&server_ep);
  printf("inicio:%lu\n",RTIMER_NOW());

  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);
  //etimer_reset(&et);

#if PLATFORM_HAS_BUTTON
#if !PLATFORM_SUPPORTS_BUTTON_HAL
  SENSORS_ACTIVATE(button_sensor);
#endif
  printf("Press a button to request %s\n", service_urls[uri_switch]);
#endif /* PLATFORM_HAS_BUTTON */

  while(1) {
    PROCESS_YIELD();

    if(etimer_expired(&et) && coap_endpoint_is_connected(&server_ep)) {
      printf("seguro:%lu\n",RTIMER_NOW());
      printf("second: %lu\n",RTIMER_SECOND);
      printf("%d\n",coap_endpoint_is_connected(&server_ep));
      printf("--Toggle timer--\n");
      
      coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
      coap_set_header_uri_path(request, service_urls[0]);

      const char msg[] = "Toggle!";

      coap_set_payload(request, (uint8_t *)msg, sizeof(msg) - 1);

      LOG_INFO_COAP_EP(&server_ep);
      LOG_INFO_("\n");

      COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);

      printf("\n--Done--\n");
    }
    else if(etimer_expired(&et)){
      if(uip_sr_num_nodes() > 0) {
      uip_sr_node_t *link;
      uip_ipaddr_t child_ipaddr;
      /* Our routing links */
      link = uip_sr_node_head();
      while(link != NULL) {
        NETSTACK_ROUTING.get_sr_node_ipaddr(&child_ipaddr, link);
        if((child_ipaddr.u8[15] == server_ep.ipaddr.u8[15])){
          if ((connect_intent == 0)) {
            LOG_INFO("alcanzable\n");
            coap_endpoint_connect(&server_ep);
            uiplib_ipaddr_print(&child_ipaddr);
            connect_intent=1;
            break;
          }
          else{
            break;
          }
          // else if (connect_intent == 10000) {
          //   printf("reintentando?\n");
          //   printf("%d \n",coap_endpoint_connect(&server_ep));
          //   connect_intent=1;
          //   break;
          // }
          // else if (connect_intent == 9000) {
          //   printf("se repite?\n");
          //   coap_endpoint_disconnect(&server_ep);
          //   connect_intent++;
          //   break;
          // }
          // else{
          //   connect_intent++;
          //   break;
          // }
          //LOG_INFO("alcanzable\n");
          
        }
        link = uip_sr_node_next(link);
      }
      } 
      etimer_reset(&et);
    }
  }

  PROCESS_END();
}
