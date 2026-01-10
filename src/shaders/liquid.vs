#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in vec3 aLight; // x=Sky, y=Block, z=AO
layout (location = 4) in vec2 aFlow;  // x, z flow direction
layout (location = 5) in float aFlags; // Bitmask: 1=Lava, 2=Source

out vec4 ourColor;
out vec2 TexCoord;
out vec3 Lighting;
out vec3 FragPos;
out vec3 FragWorldPos;
out vec2 FlowVector;
out float Fresnel;
out float WaterFlags;

uniform mat4 model;
uniform mat4 viewProjection;
// uniform mat4 view; // Removed
// uniform mat4 projection; // Removed
uniform vec3 viewPos;
uniform float u_Time;

// --- NOISE FUNCTIONS (Inlined from noise3d.ash) ---
// Modulo 289 without a division (only multiplications)
vec3 mod289(vec3 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec4 mod289(vec4 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

// Permutation polynomial: (34x^2 + x) mod 289
vec4 permute(vec4 x) {
  return mod289(((x*34.0)+1.0)*x);
}

// Taylor Inverse Square Root
vec4 taylorInvSqrt(vec4 r) {
  return 1.79284291400159 - 0.85373472095314 * r;
}

// Fade function: t^3(t(t6 - 15) + 10)
vec3 fade(vec3 t) {
  return t*t*t*(t*(t*6.0-15.0)+10.0);
}

// Classic Perlin Noise 3D
float cnoise(vec3 P) {
  vec3 Pi0 = floor(P); // Integer part for indexing
  vec3 Pi1 = Pi0 + vec3(1.0); // Integer part + 1
  Pi0 = mod289(Pi0);
  Pi1 = mod289(Pi1);
  vec3 Pf0 = fract(P); // Fractional part for interpolation
  vec3 Pf1 = Pf0 - vec3(1.0); // Fractional part - 1.0
  vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
  vec4 iy = vec4(Pi0.yy, Pi1.yy);
  vec4 iz0 = Pi0.zzzz;
  vec4 iz1 = Pi1.zzzz;

  vec4 ixy = permute(permute(ix) + iy);
  vec4 ixy0 = permute(ixy + iz0);
  vec4 ixy1 = permute(ixy + iz1);

  vec4 gx0 = ixy0 * (1.0 / 7.0);
  vec4 gy0 = fract(floor(gx0) * (1.0 / 7.0)) - 0.5;
  gx0 = fract(gx0);
  vec4 gz0 = vec4(0.5) - abs(gx0) - abs(gy0);
  vec4 sz0 = step(gz0, vec4(0.0));
  gx0 -= sz0 * (step(0.0, gx0) - 0.5);
  gy0 -= sz0 * (step(0.0, gy0) - 0.5);

  vec4 gx1 = ixy1 * (1.0 / 7.0);
  vec4 gy1 = fract(floor(gx1) * (1.0 / 7.0)) - 0.5;
  gx1 = fract(gx1);
  vec4 gz1 = vec4(0.5) - abs(gx1) - abs(gy1);
  vec4 sz1 = step(gz1, vec4(0.0));
  gx1 -= sz1 * (step(0.0, gx1) - 0.5);
  gy1 -= sz1 * (step(0.0, gy1) - 0.5);

  vec3 g000 = vec3(gx0.x,gy0.x,gz0.x);
  vec3 g100 = vec3(gx0.y,gy0.y,gz0.y);
  vec3 g010 = vec3(gx0.z,gy0.z,gz0.z);
  vec3 g110 = vec3(gx0.w,gy0.w,gz0.w);
  vec3 g001 = vec3(gx1.x,gy1.x,gz1.x);
  vec3 g101 = vec3(gx1.y,gy1.y,gz1.y);
  vec3 g011 = vec3(gx1.z,gy1.z,gz1.z);
  vec3 g111 = vec3(gx1.w,gy1.w,gz1.w);

  vec4 norm0 = taylorInvSqrt(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
  g000 *= norm0.x;
  g010 *= norm0.y;
  g100 *= norm0.z;
  g110 *= norm0.w;
  vec4 norm1 = taylorInvSqrt(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
  g001 *= norm1.x;
  g011 *= norm1.y;
  g101 *= norm1.z;
  g111 *= norm1.w;

  float n000 = dot(g000, Pf0);
  float n100 = dot(g100, vec3(Pf1.x, Pf0.yz));
  float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
  float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
  float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
  float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
  float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
  float n111 = dot(g111, Pf1);

  vec3 fade_xyz = fade(Pf0);
  vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
  vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
  float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x); 
  return 2.2 * n_xyz;
}
// --- END NOISE FUNCTIONS ---

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0f);
    
    // Unpack Flags
    int flags = int(aFlags + 0.1);
    bool isLava = (flags & 1) != 0;
    bool isSource = (flags & 2) != 0;

    // Apply Vertex Warping (Waves)
    if (!isLava) {
        float waveSpeed = 2.0;
        float waveScale = 0.5; // Spatial scale
        float heightScale = 0.1; // Height of waves

        // Simple wave logic
        vec3 noisepos = vec3(worldPos.x * waveScale, worldPos.z * waveScale, u_Time * waveSpeed);
        float wave = cnoise(noisepos);
        
        worldPos.y += wave * heightScale;
    }

    FragWorldPos = vec3(worldPos);
    gl_Position = viewProjection * worldPos;
    FragPos = vec3(worldPos);

    ourColor = aColor;
    TexCoord = aTexCoord;
    Lighting = aLight;
    FlowVector = aFlow;
    WaterFlags = aFlags;

    // Fresnel Calculation
    vec3 normal = vec3(0.0, 1.0, 0.0); // Assume flat top for now (ideally pass normal)
    vec3 viewDir = normalize(viewPos - FragWorldPos);
    
    // Bias: min opacity looking straight down
    // Scale: how fast it becomes opaque
    // Power: curve
    float bias = 0.2;
    float scale = 0.8;
    float power = 3.0;
    
    Fresnel = bias + scale * pow(1.0 + dot(-viewDir, normal), power);
    Fresnel = clamp(Fresnel, 0.0, 1.0);
    // Note: dot(viewDir, normal) is usually positive if looking at face. 
    // Here we want "glancing angle" to be high alpha.
    // dot(view, normal) -> 1.0 (looking down) -> 0.0 (looking across)
    // Fresnel = bias + scale * pow(1-dot, power);
    
    float NdotV = max(0.0, dot(normal, viewDir));
    Fresnel = bias + scale * pow(1.0 - NdotV, power);
    Fresnel = clamp(Fresnel, 0.0, 1.0);
}
