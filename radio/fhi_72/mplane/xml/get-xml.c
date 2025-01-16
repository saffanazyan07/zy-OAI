/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this file
 * except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

#include "get-xml.h"

#include <libxml/parser.h>
#include <string.h>

static bool find_ptp_status(xmlNode *node)
{
  for (xmlNode *cur_node = node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if (strcmp((const char *)cur_node->name, "sync-state") == 0 && strcmp((char *)xmlNodeGetContent(cur_node), "LOCKED") == 0) {
        printf("RU is already PTP synchronized\n");
        return true;
      }
      if (find_ptp_status(cur_node->children)) {
        return true;
      }
      void *answer = find_ru_xml_node(cur_node->children, filter);
      if (answer != NULL) {
        return answer;
      }
    }
  }
  return false;
}

bool get_ptp_sync_status(const char *buffer)
{
  // Initialize the xml file
  size_t len = strlen(buffer) + 1;
  xmlDoc *doc = xmlReadMemory(buffer, len, NULL, NULL, 0);
  xmlNode *root_element = xmlDocGetRootElement(doc);

  return find_ptp_status(root_element->children);
}
