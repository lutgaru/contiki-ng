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
#include "sys/energest.h"
#include "testglobal.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"
#include "uiplib.h"
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
static coap_observee_t *obs;
/* FIXME: This server address is hard-coded for Cooja and link-local for unconnected border router. */
#define SERVER_EP "coaps://[fd00::212:4b00:a55:dd05]"
//#define SERVER_EP "coaps://[fe80::212:4b00:a55:dd05]"
//#define SERVER_EP "coaps://[fd00::200:0:0:1]"
//#define SERVER_EP "coaps://[fd00::202:2:2:2]"
//#define SERVER_EP "coaps://[fe80::200:0:0:1]"
static struct rtimer timer_rtimer;
static int second_counter=0;

extern coap_resource_t
  test_metric;

#define TOGGLE_INTERVAL 0.001
#define NUMBERS_OF_NODES 1

PROCESS(er_example_client, "Erbium Example Client");
AUTOSTART_PROCESSES(&er_example_client);

static struct etimer et,enertimer;
rtimer_clock_t timerdiff[2];
/* Example URIs that can be queried. */
#define NUMBER_OF_URLS 5
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
char *service_urls[NUMBER_OF_URLS] =
{ "/test/hello", "/actuators/toggle", "battery/", "error/in//path", "/test/push" };
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

/*----------------------------------------------------------------------------*/
/*
 * Handle the response to the observe request and the following notifications
 */
static void
notification_callback(coap_observee_t *obs, void *notification,
                      coap_notification_flag_t flag)
{
  int len = 0;
  const uint8_t *payload = NULL;

  printf("Notification handler\n");
  printf("Observee URI: %s\n", obs->url);
  if(notification) {
    len = coap_get_payload(notification, &payload);
  }
  switch(flag) {
  case NOTIFICATION_OK:
    printf("NOTIFICATION OK: %*s\n", len, (char *)payload);
    if(len==37) printf("=,=\n");
    break;
  case OBSERVE_OK: /* server accepeted observation request */
    printf("OBSERVE_OK: %*s\n", len, (char *)payload);
    break;
  case OBSERVE_NOT_SUPPORTED:
    printf("OBSERVE_NOT_SUPPORTED: %*s\n", len, (char *)payload);
    obs = NULL;
    break;
  case ERROR_RESPONSE_CODE:
    printf("ERROR_RESPONSE_CODE: %*s\n", len, (char *)payload);
    obs = NULL;
    break;
  case NO_REPLY_FROM_SERVER:
    printf("NO_REPLY_FROM_SERVER: "
           "removing observe registration with token %x%x\n",
           obs->token[0], obs->token[1]);
    obs = NULL;
    break;
  }
}

static unsigned long
to_seconds(uint64_t time)
{
  return (unsigned long)(time / ENERGEST_SECOND);
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
  printf("%d",len);
  if(len==37) printf("=,=\n");
  printf("|%.*s", len, (char *)chunk);
}
PROCESS_THREAD(er_example_client, ev, data)
{
  PROCESS_BEGIN();
  static coap_endpoint_t server_ep[NUMBERS_OF_NODES];
  static uint8_t connect_intent = 0;
  static uint8_t muestras = 0;
  static uint8_t node_conn=0;
  static uint8_t all_conected=0;
  static uint8_t status_node[NUMBERS_OF_NODES];
  static uint8_t server_epp[NUMBERS_OF_NODES][40];
  NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER,-21);
  coap_activate_resource(&test_metric, "test/metric");
  NETSTACK_ROUTING.root_start();
  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */

  //NETSTACK_ROUTING.root_start();
  //coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);
  //coap_endpoint_connect(&server_ep);
  printf("inicio:%lu\n",RTIMER_NOW());
  etimer_set(&enertimer, CLOCK_SECOND * 10);

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

    if(etimer_expired(&enertimer)){
      etimer_reset(&enertimer);

      /* Update all energest times. */
      energest_flush();

      printf("\nEnergest:\n");
      printf(" CPU          %4lus LPM      %4lus DEEP LPM %4lus  Total time %lus\n",
            to_seconds(energest_type_time(ENERGEST_TYPE_CPU)),
            to_seconds(energest_type_time(ENERGEST_TYPE_LPM)),
            to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)),
            to_seconds(ENERGEST_GET_TOTAL_TIME()));
      printf(" Radio LISTEN %4lus TRANSMIT %4lus OFF      %4lus\n",
            to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)),
            to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)),
            to_seconds(ENERGEST_GET_TOTAL_TIME()
                        - energest_type_time(ENERGEST_TYPE_TRANSMIT)
                        - energest_type_time(ENERGEST_TYPE_LISTEN)));
    }

    if(etimer_expired(&et)){
      //printf("Terminotimer");
      if(uip_sr_num_nodes() > 0 && all_conected<NUMBERS_OF_NODES) {
        uip_sr_node_t *link;
        uip_ipaddr_t child_ipaddr;
        static uint8_t status_node[NUMBERS_OF_NODES];
        static uint8_t server_epp_temp[40];
        int node=1;
        int n;
        /* Our routing links */
        link = uip_sr_node_head();
        link = uip_sr_node_next(link);
        //printf("punto1");
        while(link != NULL) {
          NETSTACK_ROUTING.get_sr_node_ipaddr(&child_ipaddr, link);
          
          if (status_node[node-1]==NOT_EXIST){
            int lip=uiplib_ipaddr_snprint(server_epp_temp,40,&child_ipaddr);
            n = snprintf(server_epp[node-1], 40, "coaps://[%s]",server_epp_temp);
            printf("ep: %s, %d\n",server_epp[node-1],status_node[node-1]);
            coap_endpoint_parse(server_epp[node-1], n, &server_ep[node-1]);
            status_node[node-1]=EXIST;
          }
          if (!coap_endpoint_is_connected(&server_ep[node-1]) && status_node[node-1]==EXIST){
            coap_endpoint_connect(&server_ep[node-1]);
            status_node[node-1]=IN_HT;
            printf("conectando..%d\n",status_node[node-1]);
          }
          else if(coap_endpoint_is_connected(&server_ep[node-1])&& status_node[node-1]!=CONNECTED){
            printf("seguro %d: %lu\n",node,RTIMER_NOW());
            status_node[node-1]=CONNECTED;
            all_conected++;
          }
          
          node++;
          link = uip_sr_node_next(link);
        }
        etimer_reset(&et);
      }
      else if(uip_sr_num_nodes() <= 0){
        etimer_reset(&et);
      }
      else{
        obs = coap_obs_request_registration(&server_ep[0],
                                        service_urls[4], notification_callback, NULL);
        printf("#,#");
      }

      
    }
    }

  PROCESS_END();
}
