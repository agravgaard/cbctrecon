set(retry 8)
while(retry)
  math(EXPR retry "${retry} - 1")
  if(ExternalData_TIMEOUT_INACTIVITY)
    set(inactivity_timeout INACTIVITY_TIMEOUT ${ExternalData_TIMEOUT_INACTIVITY})
  elseif(
    NOT
    "${ExternalData_TIMEOUT_INACTIVITY}"
    EQUAL
    0)
    set(inactivity_timeout INACTIVITY_TIMEOUT 60)
  else()
    set(inactivity_timeout "")
  endif()
  if(ExternalData_TIMEOUT_ABSOLUTE)
    set(absolute_timeout TIMEOUT ${ExternalData_TIMEOUT_ABSOLUTE})
  elseif(
    NOT
    "${ExternalData_TIMEOUT_ABSOLUTE}"
    EQUAL
    0)
    set(absolute_timeout TIMEOUT 800)
  else()
    set(absolute_timeout "")
  endif()
  file(
    DOWNLOAD "${ExternalData_CUSTOM_LOCATION}" "${ExternalData_CUSTOM_FILE}"
    STATUS status
    LOG log
    ${inactivity_timeout} ${absolute_timeout})
  list(
    GET
    status
    0
    err)
  list(
    GET
    status
    1
    ExternalData_CUSTOM_ERROR)
  if(err)
    if("${ExternalData_CUSTOM_ERROR}" MATCHES "HTTP response code said error"
       AND "${log}" MATCHES "error: 503")
      set(ExternalData_CUSTOM_ERROR "temporarily unavailable")
    endif()
  elseif("${log}" MATCHES "\nHTTP[^\n]* 503")
    set(err TRUE)
    set(ExternalData_CUSTOM_ERROR "temporarily unavailable")
  endif()
  if(NOT err
     OR NOT
        "${ExternalData_CUSTOM_ERROR}"
        MATCHES
        "partial|timeout|temporarily")
    break()
  elseif(retry)
    message(
      STATUS "[download terminated: ${ExternalData_CUSTOM_ERROR}, retries left: ${retry}]"
    )
  endif()
endwhile()
if(${ExternalData_CUSTOM_ERROR} MATCHES "No error")
  unset(ExternalData_CUSTOM_ERROR)
endif()
