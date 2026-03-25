# CMake generated Testfile for 
# Source directory: /home/runner/work/zibby-service/zibby-service/tests
# Build directory: /home/runner/work/zibby-service/zibby-service/build-lint/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(zibby_help "/home/runner/work/zibby-service/zibby-service/build-lint/zibby-service" "--help")
set_tests_properties(zibby_help PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;1;add_test;/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;0;")
add_test(zibby_version "/home/runner/work/zibby-service/zibby-service/build-lint/zibby-service" "--version")
set_tests_properties(zibby_version PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;6;add_test;/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;0;")
add_test(zibby_message_flow_test "/home/runner/work/zibby-service/zibby-service/build-lint/tests/zibby_message_flow_test")
set_tests_properties(zibby_message_flow_test PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;32;add_test;/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;0;")
add_test(zibby_profile_peer_test "/home/runner/work/zibby-service/zibby-service/build-lint/tests/zibby_profile_peer_test")
set_tests_properties(zibby_profile_peer_test PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;55;add_test;/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;0;")
add_test(zibby_api_server_test "/home/runner/work/zibby-service/zibby-service/build-lint/tests/zibby_api_server_test")
set_tests_properties(zibby_api_server_test PROPERTIES  _BACKTRACE_TRIPLES "/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;95;add_test;/home/runner/work/zibby-service/zibby-service/tests/CMakeLists.txt;0;")
