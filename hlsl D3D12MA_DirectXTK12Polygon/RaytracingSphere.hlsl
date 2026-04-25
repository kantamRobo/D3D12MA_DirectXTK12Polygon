inline void GenerateCameraRay(uint2 index, out float3 origin, out float3 direction)
{
    float2 xy = index + 0.5f;
    float2 screenPos = xy / DispatchRaysDimensions().xy * 2.0f - 1.0f;
    screenPos.y = -screenPos.y;

    // near(0)‚æ‚èfar(1)‚Ì•û‚ª•ûŒü‚ªˆÀ’è‚µ‚â‚·‚¢
    float4 world = mul(float4(screenPos, 1.0f, 1.0f), g_sceneCB.projectionToWorld);
    world.xyz /= world.w;

    origin    = g_sceneCB.cameraPosition.xyz;
    direction = normalize(world.xyz - origin);
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 hitPosition = HitWorldPosition();

    const uint indexSizeInBytes      = 2;
    const uint indicesPerTriangle    = 3;
    const uint triangleIndexStride   = indicesPerTriangle * indexSizeInBytes;
    const uint baseIndex             = PrimitiveIndex() * triangleIndexStride;
    const uint3 indices              = Load3x16BitIndices(baseIndex);

    float3 vertexNormals[3] = {
        Vertices[indices[0]].normal,
        Vertices[indices[1]].normal,
        Vertices[indices[2]].normal
    };

    float3 triangleNormal = normalize(HitAttribute(vertexNormals, attr));

    float4 diffuseColor = CalculateDiffuseLighting(hitPosition, triangleNormal);
    float4 color        = g_sceneCB.lightAmbientColor + diffuseColor;
    payload.color = color;
}