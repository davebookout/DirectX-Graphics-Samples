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
    float2 startUV : TEXCOORD1;
    float2 endUV : TEXCOORD2;
    float2 offset : TEXCOORD3;
    int texID : TEXTURE_ID;
    float4 position : SV_POSITION;
};
struct PSInput
{
    float2 uv : TEXCOORD0;
    float2 startUV : TEXCOORD1;
    float2 endUV : TEXCOORD2;
    float2 offset : TEXCOORD3;
    int texID : TEXTURE_ID;
};

struct SampleParameters
{
    float2 offset;
    int index;
    int nothing;
    //float2 u;
    //float2 v;
    float2 start;
    float2 end;
};

cbuffer SceneConstantBuffer : register(b0)
{
    SampleParameters sampleParameters[8];  
};

//Texture2DArray g_textureArray : register(t0);
Texture2DArray g_textureAtlas : register(t0);
//texture2D g_texture : register(t2);

SamplerState g_sampler : register(s0);

VSOutput VSMain(VSInput vsIn)
{
    VSOutput result;

    // generate vertex output for a quad using a single triangle
    // triangle height and width will be 2x the quad
    // also scale the uv by 2x to handle because vertices will be outside the
    // desired quad.
    
    float2 vertexScale = float2((vsIn.index & 0x2) >> 1, (vsIn.index & 0x1));
    float2 positionOffset = vsIn.size * vertexScale * 2;
    float2x2 rotation = { cos(vsIn.rotation), -sin(vsIn.rotation), sin(vsIn.rotation), cos(vsIn.rotation) };
    float2 position = mul(rotation, (positionOffset - vsIn.size / 2));
    
    result.position = float4(vsIn.position + position, 0, 1);
    result.uv = 2 * vertexScale;
    result.startUV = sampleParameters[vsIn.texID].start;
    result.endUV = sampleParameters[vsIn.texID].end;
    result.offset = sampleParameters[vsIn.texID].offset;
    result.texID = sampleParameters[vsIn.texID].index;
    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    clip(any(input.uv.xy > 1) ? -1 : 1);
    //float3 sampleUV = float3(input.offset + input.uv.x * input.U + input.uv.y * input.V, input.texID);
    float3 sampleUV = float3(lerp(input.startUV, input.endUV, input.uv), input.texID);
    float4 result = g_textureAtlas.Sample(g_sampler, sampleUV); 
    //clip(result.a - .9);
    return result;

}
