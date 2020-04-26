#!/bin/bash

cat log_server.txt log_client.txt log_relay1.txt log_relay2.txt > log.txt
sort -k3 log.txt