#/*
# * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
# * contributor license agreements.  See the NOTICE file distributed with
# * this work for additional information regarding copyright ownership.
# * The OpenAirInterface Software Alliance licenses this file to You under
# * the OAI Public License, Version 1.1  (the "License"); you may not use this file
# * except in compliance with the License.
# * You may obtain a copy of the License at
# *
# *	  http://www.openairinterface.org/?page_id=698
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *-------------------------------------------------------------------------------
# * For more information about the OpenAirInterface (OAI) Software Alliance:
# *	  contact@openairinterface.org
# */
#---------------------------------------------------------------------

import logging
import re

# Define the mapping of physim_test values to search patterns
PHYSIM_PATTERN_MAPPING = {
	'nr_ulsim': [
		r'(Total PHY proc rx)\s+(\d+\.\d+)\s+us',  # Pattern for RX PHY processing time
		r'(ULSCH total decoding time)\s+(\d+\.\d+)\s+us',  # Pattern for ULSCH decoding time
	],
	'nr_dlsim': [
		r'(PHY proc tx)\s+(\d+\.\d+)\s+us',  # Pattern for TX PHY processing time
		r'(DLSCH encoding time)\s+(\d+\.\d+)\s+us',  # Pattern for DLSCH encoding time
	],
	'ldpctest': [
		r'(Encoding time mean):\s+(\d+\.\d+)\s+us',  # Pattern for encoding time mean
		r'(Decoding time mean):\s+(\d+\.\d+)\s+us',  # Pattern for decoding time mean
	],
}
# Define test conditions based on the simulation type
PHYSIM_TEST_CONDITION = {
	'nr_ulsim': 'test OK',  # For nr_ulsim, check if 'test OK' is present
	'nr_dlsim': 'test OK',  # For nr_dlsim, check if 'test OK' is present
	'ldpctest': None,	  # No condition for ldpctest, just process the patterns
}

class Analysis():

	def analyze_physim(log, physim_test, options, threshold):
		search_patterns = PHYSIM_PATTERN_MAPPING.get(physim_test)
		test_condition = PHYSIM_TEST_CONDITION.get(physim_test)
		success = False
		msg = ''

		try:
			with open(log, 'r') as f:
				log_content = f.read()
		except FileNotFoundError as e:
			msg = f'{log} file not found, exception: {e}'
			return False, msg
		except Exception as e:
			msg = f'Error while parsing log file {log}: exception: {e}'
			return False, msg

		if test_condition and test_condition not in log_content:
			msg = f"Test did not succeed, '{test_condition}' not found in log file {log}."
			return False, msg

		time1_match = re.search(search_patterns[0], log_content)
		time2_match = re.search(search_patterns[1], log_content)
		if not(time1_match and time2_match):
			msg = f"Processing time not found in log file {log}."
			return False, msg

		success = float(time2_match.group(2)) < float(threshold)
		if success:
			msg = f'{time1_match.group(1)}: {time1_match.group(2)} us\n{time2_match.group(1)}: {time2_match.group(2)} us'
		else:
			msg = f'{time1_match.group(1)}: {time1_match.group(2)} us\n{time2_match.group(1)}: {time2_match.group(2)} us exceeds a limit of {threshold} us'

		return success,msg
