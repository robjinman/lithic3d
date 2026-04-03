const float FOG_DISTANCE = DRAW_DISTANCE / SQRT_3;

vec4 applyFog(vec4 fragColour, float viewDistance)
{
  float t = min(1, viewDistance / FOG_DISTANCE);
  return vec4(fragColour.xyz + t * (vec3(1) - fragColour.xyz), fragColour.w);
}
