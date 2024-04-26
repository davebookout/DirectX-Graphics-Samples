//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

PSInput VSMain(float4 position : POSITION, float4 uv : TEXCOORD, uint index : SV_VertexID)
{
    PSInput result;

    result.position = position;
    result.uv = uv;
    result.uv = saturate(float2(index & 0x1, (index & 0x2)));
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    //return float4(max(input.uv - .5, 0), 0, 0);
    return g_texture.Sample(g_sampler, input.uv);
}
