set(PXR_PREFIX pxr/imaging)
set(PXR_PACKAGE hdx)

# XXX We tmp check for PXR_ENABLE_GL_SUPPORT since Hdx currently still uses
# OpenGL directly. Once Hdx uses only Hgi, remove GL_SUPPORT check.
if (NOT ${PXR_BUILD_GPU_SUPPORT} OR NOT ${PXR_ENABLE_GL_SUPPORT})
    message(STATUS
        "Skipping ${PXR_PACKAGE} because PXR_BUILD_GPU_SUPPORT is OFF")
    return()
endif()

set (optionalCppFiles "")
if (PXR_BUILD_OPENCOLORIO_PLUGIN)
    list(APPEND optionalLibs ${OCIO_LIBRARIES})
    list(APPEND optionalIncludeDirs ${OCIO_INCLUDE_DIRS})
endif()

pxr_library(hdx
    LIBRARIES
        plug
        tf
        vt
        gf
        work
        garch
        glf
        pxOsd
        hd
        hdSt
        hgi
        hgiInterop
        cameraUtil
        sdf
        ${GLEW_LIBRARY}
        ${optionalLibs}

    INCLUDE_DIRS
        ${GLEW_INCLUDE_DIR}
        ${optionalIncludeDirs}

    PUBLIC_CLASSES
        aovInputTask
        colorizeSelectionTask
        colorChannelTask
        colorCorrectionTask
        drawTargetTask
        fullscreenShader
        hgiConversions
        oitBufferAccessor
        oitRenderTask
        oitResolveTask
        oitVolumeRenderTask
        package
        pickTask
        pickFromRenderBufferTask
        presentTask
        renderTask
        renderSetupTask
        selectionTask
        selectionTracker
        shadowTask
        simpleLightTask
        task
        taskController
        tokens
        types

    PUBLIC_HEADERS
        shadowMatrixComputation.h
        version.h
        api.h

    PRIVATE_CLASSES
        debugCodes
        unitTestDelegate

    PRIVATE_HEADERS
        unitTestUtils.h

    RESOURCE_FILES
        plugInfo.json
        shaders/colorChannel.glslfx
        shaders/colorCorrection.glslfx
        shaders/fullscreen.glslfx
        shaders/renderPass.glslfx
        shaders/renderPassShader.glslfx
        shaders/renderPassDiscardUnselectedShader.glslfx
        shaders/renderPassIdShader.glslfx
        shaders/renderPassPickingShader.glslfx
        shaders/renderPassShadowShader.glslfx
        shaders/selection.glslfx
        shaders/renderPassOitShader.glslfx
        shaders/renderPassOitOpaqueShader.glslfx
        shaders/renderPassOitVolumeShader.glslfx
        shaders/oitResolveImageShader.glslfx
        shaders/outline.glslfx
        textures/StinsonBeach.exr
)

if (X11_FOUND)
pxr_build_test(testHdxRenderTask
    LIBRARIES
        hdx
        hd
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        testenv/testHdxRenderTask.cpp
)
pxr_build_test(testHdxLightAndShadow
    LIBRARIES
        hdx
        hd
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        testenv/testHdxLightAndShadow.cpp
)
pxr_build_test(testHdxIdRender
    LIBRARIES
        hdx
        hd
        hdSt
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        testenv/testHdxIdRender.cpp
)
pxr_build_test(testHdxPickAndHighlight
    LIBRARIES
        hdx
        hd
        hdSt
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        unitTestUtils.cpp
        testenv/testHdxPickAndHighlight.cpp
)
pxr_build_test(testHdxCameraAndLight
    LIBRARIES
        hdx
        hd
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        testenv/testHdxCameraAndLight.cpp
)
pxr_build_test(testHdxDrawTarget
    LIBRARIES
        hdx
        hd
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        testenv/testHdxDrawTarget.cpp
)
pxr_build_test(testHdxPickingWithReprAndRefineChanges
    LIBRARIES
        hdx
        hd
        hdSt
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        unitTestUtils.cpp
        testenv/testHdxPickingWithReprAndRefineChanges.cpp
)
pxr_build_test(testHdxUnpickablesAsOccluders
    LIBRARIES
        hdx
        hd
        hdSt
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        unitTestUtils.cpp
        testenv/testHdxUnpickablesAsOccluders.cpp
)
pxr_build_test(testHdxPickSubprims
    LIBRARIES
        hdx
        hd
        hdSt
        sdf
        glf
        garch
        arch
        tf
    CPPFILES
        unitTestUtils.cpp
        testenv/testHdxPickSubprims.cpp
)
endif()
