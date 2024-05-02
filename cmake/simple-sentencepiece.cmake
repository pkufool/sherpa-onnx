function(download_simple_sentencepiece)
  include(FetchContent)

  set(simple-sentencepiece_URL "https://github.com/pkufool/simple-sentencepiece/archive/refs/tags/v0.3.tar.gz")
  set(simple-sentencepiece_URL2 "https://hub.nauu.cf/pkufool/simple-sentencepiece/archive/refs/tags/v0.3.tar.gz")
  set(simple-sentencepiece_HASH "SHA256=21525faa5dd77deb799e82db98999b718933d7af507dc2b67f2f4862353f1c72")

  # If you don't have access to the Internet,
  # please pre-download simple-sentencepiece
  set(possible_file_locations
    $ENV{HOME}/Downloads/simple-sentencepiece-0.3.tar.gz
    ${CMAKE_SOURCE_DIR}/simple-sentencepiece-0.3.tar.gz
    ${CMAKE_BINARY_DIR}/simple-sentencepiece-0.3.tar.gz
    /tmp/simple-sentencepiece-0.3.tar.gz
    /star-fj/fangjun/download/github/simple-sentencepiece-0.3.tar.gz
  )

  foreach(f IN LISTS possible_file_locations)
    if(EXISTS ${f})
      set(simple-sentencepiece_URL  "${f}")
      file(TO_CMAKE_PATH "${simple-sentencepiece_URL}" simple-sentencepiece_URL)
      message(STATUS "Found local downloaded simple-sentencepiece: ${simple-sentencepiece_URL}")
      set(simple-sentencepiece_URL2)
      break()
    endif()
  endforeach()

  FetchContent_Declare(simple-sentencepiece
    URL
      ${simple-sentencepiece_URL}
      ${simple-sentencepiece_URL2}
    URL_HASH
      ${simple-sentencepiece_HASH}
  )

  FetchContent_GetProperties(simple-sentencepiece)
  if(NOT simple-sentencepiece_POPULATED)
    message(STATUS "Downloading simple-sentencepiece ${simple-sentencepiece_URL}")
    FetchContent_Populate(simple-sentencepiece)
  endif()
  message(STATUS "simple-sentencepiece is downloaded to ${simple-sentencepiece_SOURCE_DIR}")
  add_subdirectory(${simple-sentencepiece_SOURCE_DIR} ${simple-sentencepiece_BINARY_DIR} EXCLUDE_FROM_ALL)

  target_include_directories(ssentencepiece_core
    PUBLIC
      ${simple-sentencepiece_SOURCE_DIR}/
  )
  set_target_properties(ssentencepiece_core PROPERTIES OUTPUT_NAME "sherpa-onnx-ssentencepiece-core")
endfunction()

download_simple_sentencepiece()
