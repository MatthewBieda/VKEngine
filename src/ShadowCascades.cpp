#include "ShadowCascades.hpp"
#include <gtc/matrix_transform.hpp>
#include <iostream>
#include <limits>
#include <algorithm>
#include <cmath>

void ShadowCascades::updateCascades(
    const glm::vec3& camPos,
    const glm::vec3& camFront,
    const glm::vec3& camUp,
    const glm::vec3& camRight,
    float fov,
    float aspect,
    const glm::vec3& lightDir,
    float nearPlane,
    float farPlane,
    float lambda)
{
    // Compute split depths (absolute)
    m_splitDepths.resize(NUM_CASCADES);
    float clipRange = farPlane - nearPlane;
    float ratio = farPlane / nearPlane;
    for (uint32_t i = 0; i < NUM_CASCADES; ++i)
    {
        float p = (i + 1) / static_cast<float>(NUM_CASCADES);
        float log = nearPlane * std::pow(ratio, p);
        float uniform = nearPlane + clipRange * p;
        m_splitDepths[i] = lambda * log + (1.0f - lambda) * uniform;
    }

    glm::vec3 lightDirNormalized = glm::normalize(lightDir);
    m_cascades.resize(NUM_CASCADES);

    float lastSplit = nearPlane;
    for (uint32_t i = 0; i < NUM_CASCADES; ++i)
    {
        float cascadeNear = lastSplit;
        float cascadeFar = m_splitDepths[i];

        std::vector<glm::vec3> frustumCorners = getCascadeFrustumCorners(
            camPos, camFront, camUp, camRight, fov, aspect, cascadeNear, cascadeFar
        );

        m_cascades[i].viewProj = calculateLightMatrix(frustumCorners, lightDirNormalized);
        m_cascades[i].nearDepth = cascadeNear;
        m_cascades[i].farDepth = cascadeFar;

        lastSplit = cascadeFar;
    }
}

std::vector<glm::vec3> ShadowCascades::getCascadeFrustumCorners(
    const glm::vec3& camPos,
    const glm::vec3& camFront,
    const glm::vec3& camUp,
    const glm::vec3& camRight,
    float fov,
    float aspect,
    float nearPlane,
    float farPlane)
{
    std::vector<glm::vec3> corners(8);

    float tanHalfVFOV = tanf(glm::radians(fov * 0.5f));
    float tanHalfHFOV = tanHalfVFOV * aspect;

    // Compute centers of near and far planes
    glm::vec3 nc = camPos + camFront * nearPlane;
    glm::vec3 fc = camPos + camFront * farPlane;

    // Compute half-sizes of near/far planes
    float nearHalfHeight = tanHalfVFOV * nearPlane;
    float nearHalfWidth = tanHalfHFOV * nearPlane;
    float farHalfHeight = tanHalfVFOV * farPlane;
    float farHalfWidth = tanHalfHFOV * farPlane;

    // Build frustum corners
    corners[0] = nc + camUp * nearHalfHeight - camRight * nearHalfWidth; // near top-left
    corners[1] = nc + camUp * nearHalfHeight + camRight * nearHalfWidth; // near top-right
    corners[2] = nc - camUp * nearHalfHeight - camRight * nearHalfWidth; // near bottom-left
    corners[3] = nc - camUp * nearHalfHeight + camRight * nearHalfWidth; // near bottom-right

    corners[4] = fc + camUp * farHalfHeight - camRight * farHalfWidth;   // far top-left
    corners[5] = fc + camUp * farHalfHeight + camRight * farHalfWidth;   // far top-right
    corners[6] = fc - camUp * farHalfHeight - camRight * farHalfWidth;   // far bottom-left
    corners[7] = fc - camUp * farHalfHeight + camRight * farHalfWidth;   // far bottom-right

    glm::vec3 minC = corners[0], maxC = corners[0];
    for (int i = 1; i < 8; ++i) {
        minC = glm::min(minC, corners[i]);
        maxC = glm::max(maxC, corners[i]);
    }
    glm::vec3 size = maxC - minC;

    return corners;
}

glm::mat4 ShadowCascades::calculateLightMatrix(
    const std::vector<glm::vec3>& frustumCorners,
    const glm::vec3& lightDirNormalized)
{
    // 1. Compute frustum center
    glm::vec3 center(0.0f);
    for (const auto& v : frustumCorners) center += v;
    center /= static_cast<float>(frustumCorners.size());

    // 2. Compute light view
    glm::vec3 up(0.0f, 1.0f, 0.0f);
    // 3. Compute temporary light view
    glm::mat4 lightViewTemp = glm::lookAt(center - lightDirNormalized * 1.0f, center, up);

    // 4. Transform frustum corners into light space
    std::vector<glm::vec3> cornersLS;
    cornersLS.reserve(frustumCorners.size());
    for (const auto& v : frustumCorners)
        cornersLS.push_back(glm::vec3(lightViewTemp * glm::vec4(v, 1.0f)));

    // 5. Compute bounds in light space
    glm::vec3 minLS(std::numeric_limits<float>::max());
    glm::vec3 maxLS(std::numeric_limits<float>::lowest());
    for (const auto& v : cornersLS)
    {
        minLS = glm::min(minLS, v);
        maxLS = glm::max(maxLS, v);
    }

    glm::vec3 extents = maxLS - minLS;

    // 6. Calculate Texel Size
    constexpr float SHADOW_MAP_SIZE = 4096.0f;

    // Save pre-snapped bounds for comparison
    float preSnapExtentsX = extents.x;
    float preSnapExtentsY = extents.y;

    // The texel size in LIGHT SPACE UNITS (used for snapping the position)
    float texelSizeX = extents.x / SHADOW_MAP_SIZE;
    float texelSizeY = extents.y / SHADOW_MAP_SIZE;

    // Calculate the light-space center before snapping
    glm::vec3 centerLS_preSnap = (minLS + maxLS) * 0.5f;

    // 6a. Snap the center's X/Y coordinates (CRITICAL for stability)
    centerLS_preSnap.x = floor(centerLS_preSnap.x / texelSizeX + 0.5f) * texelSizeX;
    centerLS_preSnap.y = floor(centerLS_preSnap.y / texelSizeY + 0.5f) * texelSizeY;

    // 6b. Recalculate min/max using the snapped center and the ORIGINAL extents.
    minLS.x = centerLS_preSnap.x - preSnapExtentsX * 0.5f;
    maxLS.x = centerLS_preSnap.x + preSnapExtentsX * 0.5f;
    minLS.y = centerLS_preSnap.y - preSnapExtentsY * 0.5f;
    maxLS.y = centerLS_preSnap.y + preSnapExtentsY * 0.5f;

    // 6c. Recalculate extents (for padding and debug output)
    extents = maxLS - minLS;

    // 7. Add a small padding
    // float pad = 0.05f * glm::max(extents.x, extents.y);
    // minLS.x -= pad; maxLS.x += pad;
    // minLS.y -= pad; maxLS.y += pad;

    // 8. Compute light Z range dynamically
    float zNear = minLS.z;
    float zFar = maxLS.z;

    //Enforce a minimum Z range for depth precision
    float minZRange = 100.0f;
    if (zFar - zNear < minZRange)
    {
        zFar = zNear + minZRange;
    }

    // 9. Recompute light position based on Z range
    glm::vec3 centerLS = (minLS + maxLS) * 0.5f;
    glm::vec3 lightPos = center - lightDirNormalized * ((zFar - zNear) * 0.5f + 1.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, center, up);

    // 10. Final orthographic projection
    glm::mat4 lightProj = glm::ortho(minLS.x, maxLS.x, minLS.y, maxLS.y, zNear, zFar);
    lightProj[1][1] *= -1.0f; // Vulkan Y flip

    return lightProj * lightView;
}
