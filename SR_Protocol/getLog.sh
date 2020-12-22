#!/bin/bash

cat log_server.txt log_client.txt log_relay1.txt log_relay2.txt > logTmp.txt
sort -k3 logTmp.txt > log.txt
rm logTmp.txt
cat log.txt