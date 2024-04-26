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

struct VSInput
{
    float2 position : POSITION;
    float2 size : SIZE;
    float rotation : ROTATION;
    int texID : TEXTURE_ID;
    uint index : SV_VertexID;
};

struct VSOutput
{
    float2 uv : TEXCOORD;
    float2 U : TEXCOORD1;
    float2 V : TEXCOORD2;
    int texID : TEXTURE_ID;
    float4 position : SV_POSITION;
};
struct PSInput
{
    float2 uv : TEXCOORD0;
    float2 U : TEXCOORD1;
    float2 V : TEXCOORD2;
    int texID : TEXTURE_ID;
};

struct SampleParameters
{
    int2 offset;
    int index;
    int nothing;
    float2 u;
    float2 v;
};

cbuffer SceneConstantBuffer : register(b0)
{
    SampleParameters sampleParameters[8];  
};

Texture2DArray g_textureArray : register(t0);
//Texture2DArray g_textureAtlas : register(t1);
//texture2D g_texture : register(t2);

SamplerState g_sampler : register(s0);

VSOutput VSMain(VSInput vsIn)
{
    VSOutput result;

    //result.position = float4(vsIn.position + vsIn.size * float2(vsIn.index & 0x1, vsIn.index & 0x2), .5, 1);
    
    float2 offset = vsIn.size * float2((vsIn.index & 0x2) >> 1, vsIn.index & 0x1);
    result.position = float4(vsIn.position + offset, 0, 1);
    result.uv = 2 * float2((vsIn.index & 0x2) >> 1, (vsIn.index & 0x1));
    result.U = sampleParameters[vsIn.texID].u;
    result.V = sampleParameters[vsIn.texID].v;
    result.texID = vsIn.texID;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    clip(any(input.uv.xy > 1) ? -1 : 1);
    //return float4(input.uv, 0, 1);
    float3 sampleUV = float3(input.uv.x * input.U + input.uv.y * input.V, input.texID);
    return g_textureArray.Sample(g_sampler, sampleUV);
}
