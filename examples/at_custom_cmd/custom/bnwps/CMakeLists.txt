# BNWPS Component CMakeLists.txt

set(bnwps_srcs "")

# Add BNWPS sources if enabled

list(APPEND bnwps_srcs "bnwps/at_cmd_bnwps.c")
list(APPEND bnwps_srcs "bnwps/bnwps_sm.c")


# Define BNWPS include directories
set(bnwps_includes "bnwps")

# Return the sources and includes for use by parent CMakeLists.txt
set(BNWPS_SRCS ${bnwps_srcs} PARENT_SCOPE)
set(BNWPS_INCLUDES ${bnwps_includes} PARENT_SCOPE)
