/*
* Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "include/Common.hlsl"
#include "../include/Lights.h"

float SpotAttenuation(float3 spotDirection, float3 lightDirection, float umbra, float penumbra)
{
	// Spot attenuation function from Frostbite, pg 115 in RTR4
	float cosTheta = saturate(dot(spotDirection, lightDirection));
	float t = saturate((cosTheta - cos(umbra)) / (cos(penumbra) - cos(umbra)));
	return t * t;
}

float LightWindowing(float distanceToLight, float maxDistance)
{
	return pow(saturate(1.f - pow((distanceToLight / maxDistance), 4)), 2);
}

float LightFalloff(float distanceToLight)
{
	return 1.f / pow(max(distanceToLight, 1.f), 2);
}

/**
* Computes the visibility factor for a given vector to a light.
*/
float LightVisibility(float3 worldPosition, float3 lightVector, float3 normal, float normalBias, float viewBias, RaytracingAccelerationStructure bvh)
{
	RayDesc ray;
	ray.Origin = worldPosition + (normal * normalBias);
	ray.Direction = normalize(lightVector);
	ray.TMin = 0.f;
	ray.TMax = length(lightVector) - viewBias;

	PayloadData payload = (PayloadData)0;
	TraceRay(
		bvh,
		RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
		0x01,
		0,
		1,
		0,
		ray,
		payload);

	return (payload.hitT < 0.f);
}

/**
* Evaluate direct lighting and showing for the current surface and the spot light.
*/
float3 EvaluateSpotLight(float3 worldPosition, float3 normal, float normalBias, float viewBias, RaytracingAccelerationStructure bvh)
{
	float3 lightVector = (spotLight.position - worldPosition);
	float  lightDistance = length(lightVector);
	
	// Early out, light energy doesn't reach the surface
	if (lightDistance > spotLight.maxDistance) return 0.f;

	float  visibility = LightVisibility(worldPosition, lightVector, normal, normalBias, viewBias, bvh);

	// Early out, the light isn't visible from the surface
	if (visibility <= 0) return 0.f;

	// Compute lighting
	float3 lightDirection = normalize(lightVector);
	float  nol = max(dot(normal, lightDirection), 0.f);
	float3 spotDirection = normalize(spotLight.direction);
	float  attenuation = SpotAttenuation(spotDirection, -lightDirection, spotLight.umbraAngle, spotLight.penumbraAngle);
	float  falloff = LightFalloff(lightDistance);
	float  window = LightWindowing(lightDistance, spotLight.maxDistance);
	
	return spotLight.power * spotLight.color * nol * attenuation * falloff * window * visibility;
}

/**
* Evaluate direct lighting and showing for the current surface and the point light.
*/
float3 EvaluatePointLight(float3 worldPosition, float3 normal, float normalBias, float viewBias, RaytracingAccelerationStructure bvh)
{
	float3 lightVector = (pointLight.position - worldPosition);
	float  lightDistance = length(lightVector);

	// Early out, light energy doesn't reach the surface
	if (lightDistance > pointLight.maxDistance) return 0.f;

	float  visibility = LightVisibility(worldPosition, lightVector, normal, normalBias, viewBias, bvh);

	// Early out, the light isn't visible from the surface
	if (visibility <= 0) return float3(0.f, 0.f, 0.f);

	// Compute lighting
	float3 lightDirection = normalize(lightVector);
	float  nol = max(dot(normal, lightDirection), 0.f);
	float  falloff = LightFalloff(lightDistance);
	float  window = LightWindowing(lightDistance, pointLight.maxDistance);

	return pointLight.power * pointLight.color * nol * falloff * window * visibility;
}

/**
* Evaluate direct lighting and showing for the current surface and the directional light.
*/
float3 EvaluateDirectionalLight(float3 worldPosition, float3 normal)
{
	return float3(0.f, 0.f, 0.f);
}

/**
* Computes the diffuse contribution of a surface to the final color (direct lighting).
*/
float3 DirectDiffuseLighting(float3 baseColor, float3 worldPosition, float3 normal, float normalBias, float viewBias, RaytracingAccelerationStructure bvh)
{
	float3 albedo = (baseColor / PI);
	float3 lighting = 0.f;
	
	if (lightMask & 0x00000001)
	{
		lighting += EvaluateSpotLight(worldPosition, normal, normalBias, viewBias, bvh);
	}

	if (lightMask & 0x00000010)
	{
		lighting += EvaluatePointLight(worldPosition, normal, normalBias, viewBias, bvh);
	}

	if (lightMask & 0x00000100)
	{
		lighting += EvaluateDirectionalLight(worldPosition, normal);
	}

	lighting = saturate(lighting);
	return saturate(albedo * lighting);
}
