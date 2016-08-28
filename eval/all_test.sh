#!/bin/sh

bash auto_test_all.sh

./modify_server.sh 3 5

bash auto_test_all.sh

./modify_server.sh 5 7
bash auto_test_all.sh

./modify_server.sh 7 9
bash auto_test_all.sh

./modify_server.sh 9 11
bash auto_test_all.sh

./modify_server.sh 11 13
bash auto_test_all.sh

./modify_server.sh 13 15
bash auto_test_all.sh

./modify_server.sh 15 17
bash auto_test_all.sh

