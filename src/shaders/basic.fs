#version 330 core
out vec4 FragColor;

in vec4 ourColor;
in vec2 TexCoord;
in vec3 Lighting;
in vec4 TexOrigin;
in vec3 FragPos;
in vec3 Climate;
in vec4 OverlayOrigin;
in float OverlayEnabled;

// texture sampler
uniform sampler2D texture1;
uniform sampler2D tintMaps[8]; // Legacy/Fallback samplers
uniform vec4 u_TintRects[8]; // Atlas UV Rects (if z > 0, use Atlas)
uniform bool useTexture;
uniform float sunStrength;
uniform bool useLighting;
uniform vec3 viewPos; // Camera Position for Fog

// Phase 4: Heatmap
uniform bool useHeatmap;

// Phase 4: Fog
uniform bool useFog;
uniform float fogDist;
uniform vec3 fogColor;

void main()
{
    // Tiling Logic
    // TexCoord contains 0..Width, 0..Height
    // TexOrigin contains standard Atlas UVs (e.g. 0.25, 0.5)
    // We Map 0..1 sub-tile to 0..0.25 atlas space
    
    vec2 tileUV = fract(TexCoord);
    
    // Dynamic UV Scale from Vertex Attribute
    vec2 finalUV = TexOrigin.xy + vec2(tileUV.x * TexOrigin.z, tileUV.y * TexOrigin.w);
    
    if (useHeatmap) {
        // Red = Dark, Green = Light
        // Mix based on max light
        float val = max(Lighting.x * sunStrength, Lighting.y);
        FragColor = mix(vec4(1.0, 0.0, 0.0, 1.0), vec4(0.0, 1.0, 0.0, 1.0), val);
        return;
    }
    
    // Lighting.x = SkyLight (0-1)
    // Lighting.y = BlockLight (0-1)
    
    // Phase 2: Multiply SkyLight by sunBrightness uniform
    float sunLevel = Lighting.x * sunStrength; 
    float blockLevel = Lighting.y;
    
    // Phase 4: AO
    // Optimized: Intensity tuned to avoid needing clamp
    float aoVal = Lighting.z; // 0, 1, 2, 3
    float aoFactor = 1.0 - (aoVal * 0.22); // Range: 1.0 to 0.34
    
    float lightVal = max(sunLevel, blockLevel);
    lightVal = max(0.2, lightVal); // Ambient min

    if (!useLighting) lightVal = 1.0; // Bypass for UI/Fullbright

    // Apply Tint from Climate Data
    // Climate.z = Tint Index (0 = None, 1+ = Map Index)
    int tintIndex = int(Climate.z + 0.1); // Round safe
    
    vec4 tintColor = vec4(1.0);
    if (tintIndex > 0) {
        // Shared UV logic (Legacy uses 1.0 - y)
        vec2 localUV = vec2(Climate.x, 1.0 - Climate.y);
        
        if (tintIndex <= 8) { 
             int idx = tintIndex - 1;
             vec4 rect = u_TintRects[idx];
             
             if (rect.z > 0.0) {
                 vec2 tintAtlasUV = rect.xy + localUV * rect.zw;
                 tintColor = texture(texture1, tintAtlasUV);
             } else {
                 switch(idx) {
                     case 0: tintColor = texture(tintMaps[0], localUV); break;
                     case 1: tintColor = texture(tintMaps[1], localUV); break;
                     case 2: tintColor = texture(tintMaps[2], localUV); break;
                     case 3: tintColor = texture(tintMaps[3], localUV); break;
                     case 4: tintColor = texture(tintMaps[4], localUV); break;
                     case 5: tintColor = texture(tintMaps[5], localUV); break;
                     case 6: tintColor = texture(tintMaps[6], localUV); break;
                     case 7: tintColor = texture(tintMaps[7], localUV); break;
                     default: tintColor = vec4(1.0); break;
                 }
             }
        }
    }

    // Overlay & Tinting Logic
    // OverlayEnabled bits: 0: hasOverlay, 1: tintOverlay, 2: tintBase
    int overlayFlags = int(OverlayEnabled + 0.1);
    bool hasOverlay = (overlayFlags & 1) != 0;
    bool tintOverlay = (overlayFlags & 2) != 0;
    bool tintBase = (overlayFlags & 4) != 0;

    vec4 texColor;
    vec4 baseTexColor = texture(texture1, finalUV);
    if (tintBase && tintIndex > 0) {
        baseTexColor *= tintColor;
    }

    if (hasOverlay) {
        vec2 overlayTileUV = fract(TexCoord);
        vec2 overlayUV = OverlayOrigin.xy + vec2(overlayTileUV.x * OverlayOrigin.z, overlayTileUV.y * OverlayOrigin.w);
        vec4 overlayTexColor = texture(texture1, overlayUV);
        
        if (tintOverlay && tintIndex > 0) {
            overlayTexColor *= tintColor;
        }
        
        texColor = mix(baseTexColor, overlayTexColor, overlayTexColor.a);
    } else {
        texColor = baseTexColor;
    }

    if (!useTexture)
        texColor = vec4(1.0, 1.0, 1.0, 1.0); // Use white if no texture

    if (texColor.a < 0.1) discard;

    // Combine texture color with vertex color (tint)
    vec4 result = texColor * ourColor; 
    
    // Apply lighting
    vec4 finalColor = vec4(result.rgb * lightVal * aoFactor, result.a);
    
    if (useFog) {
        float distance = length(viewPos - FragPos);
        float fogFactor = exp(-pow((distance / fogDist), 2.0)); 
        fogFactor = clamp(fogFactor, 0.0, 1.0);
        
        finalColor = mix(vec4(fogColor, 1.0), finalColor, fogFactor);
    }
    FragColor = finalColor;
}
