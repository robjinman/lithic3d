const float FOG_DISTANCE = 10000.0;

vec4 applyFog(vec4 fragColour, float viewDistance)
{
  float t = min(1, viewDistance / FOG_DISTANCE);
  return vec4(fragColour.xyz + t * (vec3(1) - fragColour.xyz), fragColour.w);
}
