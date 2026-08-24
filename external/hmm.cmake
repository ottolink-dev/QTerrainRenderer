if(NOT TARGET hmm)
  include(FetchContent)

  FetchContent_Declare(
    hmm
    GIT_REPOSITORY https://github.com/fogleman/hmm.git
    GIT_TAG        master
  )

  FetchContent_GetProperties(hmm)
  if(NOT hmm_POPULATED)
    FetchContent_Populate(hmm)

    set(HMM_DIR ${hmm_SOURCE_DIR}/src)
    set(HMM_SRC
        ${HMM_DIR}/base.cpp
        ${HMM_DIR}/blur.cpp
        ${HMM_DIR}/heightmap.cpp
        ${HMM_DIR}/triangulator.cpp
    )

    add_library(hmm STATIC ${HMM_SRC})
    add_library(hmm::hmm ALIAS hmm)

    target_include_directories(hmm PUBLIC ${hmm_SOURCE_DIR})
    target_link_libraries(hmm glm::glm)
  endif()
endif()
