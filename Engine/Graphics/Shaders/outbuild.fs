#version 330 core

in vec4 vertexColour;
in vec2 texuv;
flat in vec2 olayer;
in vec3 vsPos;
in vec3 vsNorm;
flat in int vsAttrib;

out vec4 FragColour;

struct Sunlight {
	vec3 direction;

	vec3 ambient;
	vec3 diffuse;
	vec3 specular;
};

struct PointLight {
    
    float type;  // 0- off, 1- stat, 2 - mob

    vec3 position;
    
    //float constant;
    //float linear;
    //float quadratic;


    float radius;
	
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform int waterframe;
uniform Sunlight sun;
uniform vec3 CameraPos;
uniform int flowtimer;
uniform int watertiles;

#define num_point_lights 20
uniform PointLight fspointlights[num_point_lights];

layout (binding = 0) uniform sampler2DArray textureArray0;
layout (binding = 1) uniform sampler2DArray textureArray1;
layout (binding = 2) uniform sampler2DArray textureArray2;
layout (binding = 3) uniform sampler2DArray textureArray3;
layout (binding = 4) uniform sampler2DArray textureArray4;
layout (binding = 5) uniform sampler2DArray textureArray5;
layout (binding = 6) uniform sampler2DArray textureArray6;
layout (binding = 7) uniform sampler2DArray textureArray7;
layout (binding = 8) uniform sampler2DArray textureArray8;
layout (binding = 9) uniform sampler2DArray textureArray9;
layout (binding = 10) uniform sampler2DArray textureArray10;
layout (binding = 11) uniform sampler2DArray textureArray11;
layout (binding = 12) uniform sampler2DArray textureArray12;
layout (binding = 13) uniform sampler2DArray textureArray13;
layout (binding = 14) uniform sampler2DArray textureArray14;
layout (binding = 15) uniform sampler2DArray textureArray15;




// funcs
vec3 CalcSunLight(Sunlight light, vec3 normal, vec3 viewDir, vec3 thisfragcol);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir);

void main() {
	
	vec3 fragnorm = normalize(vsNorm);
    vec3 fragviewdir = normalize(CameraPos - vsPos);

    // vec4 watercol = texture(textureArray0, vec3(texuv.x,texuv.y,waterframe));

	vec4 fragcol = vec4(0);

    vec2 texcoords = vec2(0);
    vec2 texuvmod = vec2(0);
    vec2 deltas = vec2(0);

    ///*
    
                      //      unsigned int flow_anim_timer = OS_GetTime() >> 4;
                      //  unsigned int flow_u_mod = poly->texture->GetWidth() - 1;
                      //  unsigned int flow_v_mod = poly->texture->GetHeight() - 1;

                        if (abs(vsNorm.z) >= 0.9) {
                            if (vsAttrib & 0x400) texuvmod.y = 1;
                                //poly->sTextureDeltaV += flow_anim_timer & flow_v_mod;
                            if (vsAttrib & 0x800) texuvmod.y = -1;
                                //poly->sTextureDeltaV -= flow_anim_timer & flow_v_mod;
                        } else {
                            if (vsAttrib & 0x400) texuvmod.y = -1;
                                //poly->sTextureDeltaV -= flow_anim_timer & flow_v_mod;
                            if (vsAttrib & 0x800) texuvmod.y = 1;
                                //poly->sTextureDeltaV += flow_anim_timer & flow_v_mod;
                        }

                        if (vsAttrib & 0x1000) {
                            texuvmod.x = -1;    
                        //poly->sTextureDeltaU -= flow_anim_timer & flow_u_mod;
                        } else if (vsAttrib & 0x2000) {
                            texuvmod.x = 1;    
                        //poly->sTextureDeltaU += flow_anim_timer & flow_u_mod;
                        }
    //*/







//   if (olayer.x == 15) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray15,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray15,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray15,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray15,0).y; 
//        fragcol = texture(textureArray15, vec3(texcoords.x,texcoords.y,olayer.y));
//    }
    
//    if (olayer.x == 14) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray14,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray14,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray14,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray14,0).y; 
//        fragcol = texture(textureArray14, vec3(texcoords.x,texcoords.y,olayer.y));
//    }

//    if (olayer.x == 13) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray13,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray13,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray13,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray13,0).y; 
//        fragcol = texture(textureArray13, vec3(texcoords.x,texcoords.y,olayer.y));
//    }

//    if (olayer.x == 12) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray12,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray12,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray12,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray12,0).y; 
//        fragcol = texture(textureArray12, vec3(texcoords.x,texcoords.y,olayer.y));
//    }

//    if (olayer.x == 11) {
//       deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray11,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray11,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray11,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray11,0).y; 
//        fragcol = texture(textureArray11, vec3(texcoords.x,texcoords.y,olayer.y));
//    }
    
//    if (olayer.x == 10) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray10,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray10,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray10,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray10,0).y; 
//        fragcol = texture(textureArray10, vec3(texcoords.x,texcoords.y,olayer.y));
//    }
    
//   if (olayer.x == 9) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray9,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray9,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray9,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray9,0).y; 
//        fragcol = texture(textureArray9, vec3(texcoords.x,texcoords.y,olayer.y));
//    }

//    if (olayer.x == 8) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray8,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray8,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray8,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray8,0).y; 
//        fragcol = texture(textureArray8, vec3(texcoords.x,texcoords.y,olayer.y));
//    }

//    if (olayer.x == 7) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray7,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray7,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray7,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray7,0).y; 
//        fragcol = texture(textureArray7, vec3(texcoords.x,texcoords.y,olayer.y));
//    }

//    if (olayer.x == 6) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray6,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray6,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray6,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray6,0).y; 
//        fragcol = texture(textureArray6, vec3(texcoords.x,texcoords.y,olayer.y));
//    }
    
//    if (olayer.x == 5) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray5,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray5,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray5,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray5,0).y; 
//        fragcol = texture(textureArray5, vec3(texcoords.x,texcoords.y,olayer.y));
//    }
    
//    if (olayer.x == 4) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray4,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray4,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray4,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray4,0).y; 
//        fragcol = texture(textureArray4, vec3(texcoords.x,texcoords.y,olayer.y));
//    }
    
//    if (olayer.x == 3) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray3,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray3,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray3,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray3,0).y; 
//        fragcol = texture(textureArray3, vec3(texcoords.x,texcoords.y,olayer.y));
//    }
    
//    if (olayer.x == 2) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray2,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray2,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray2,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray2,0).y; 
//        fragcol = texture(textureArray2, vec3(texcoords.x,texcoords.y,olayer.y));
//    }

//   if (olayer.x == 1) {
//        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray1,0).x-1));
//        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray1,0).y-1));
//        texcoords.x = (deltas.x + texuv.x) / textureSize(textureArray1,0).x;
//        texcoords.y = (deltas.y + texuv.y) / textureSize(textureArray1,0).y; 
//        fragcol = texture(textureArray1, vec3(texcoords.x,texcoords.y,olayer.y));
//    }
   


//    if (olayer.x == 0) {
        deltas.x = texuvmod.x * (flowtimer & int(textureSize(textureArray0,0).x-1));
        deltas.y = texuvmod.y * (flowtimer & int(textureSize(textureArray0,0).y-1));
        texcoords.x = (deltas.x*4.0 + texuv.x) / textureSize(textureArray0,0).x;
        texcoords.y = (deltas.y*4.0 + texuv.y) / textureSize(textureArray0,0).y; 
        fragcol = texture(textureArray0, vec3(texcoords.x,texcoords.y,olayer.y));

        vec4 toplayer = texture(textureArray0, vec3(texcoords.x/4.0,texcoords.y/4.0,0));
        vec4 watercol = texture(textureArray0, vec3(texcoords.x/4.0,texcoords.y/4.0,waterframe));

        if (watertiles == 1 && olayer.y == 0){
            if ((vsAttrib & 0x3C00) != 0){
                fragcol = toplayer;
            } else {
                fragcol = watercol;
            }
        }
//   }

    //if (((vsAttrib & 2) != 0) && (fragcol.a == 0)){
    //    fragcol = watercol;
    //}




//	if (olayer.y == 0){
//		//FragColour = texture(textureArray, vec3(texuv.x,texuv.y,waterframe));
//		fragcol = texture(textureArray0, vec3(texuv.x,texuv.y,waterframe));
//	} else {
//		//FragColour = texture(textureArray, vec3(texuv.x,texuv.y,olayer));
//		fragcol = texture(textureArray0, vec3(texuv.x,texuv.y,olayer.y));
//		//if (FragColour.a == 0){
//		if (fragcol.a == 0){
//			//FragColour = texture(textureArray, vec3(texuv.x,texuv.y,waterframe));
//			fragcol = texture(textureArray0, vec3(texuv.x,texuv.y,waterframe));
//		}
//	}





	vec3 result = CalcSunLight(sun, fragnorm, fragviewdir, vec3(1)); //fragcol.rgb);
    result = clamp(result, 0, 0.85);

    result += CalcPointLight(fspointlights[0], fragnorm, vsPos, fragviewdir);

    // stack stationary
    for(int i = 1; i < num_point_lights; i++) {
        if (fspointlights[i].type == 1)
            result += CalcPointLight(fspointlights[i], fragnorm, vsPos, fragviewdir);
    }

    result *= fragcol.rgb;

    // stack mobile

    for(int i = 1; i < num_point_lights; i++) {
        if (fspointlights[i].type == 2)
            result += CalcPointLight(fspointlights[i], fragnorm, vsPos, fragviewdir);
    }
        
    vec3 clamps = result; // fragcol.rgb *  // clamp(result,0,1) * ; 

    vec3 dull;

    if (vsAttrib & 0x10000) {
        float ss = (sin(flowtimer/30.0)+ 1.0) / 2.0;
        dull = vec3(1, ss, ss);
    } else {
        dull = vec3(1,1,1);
    }

	FragColour = vec4(clamps,1) * vec4(dull,1); // result, 1.0);

}

// calculates the color when using a directional light.
vec3 CalcSunLight(Sunlight light, vec3 normal, vec3 viewDir, vec3 thisfragcol) {
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
   float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128.0 ); //material.shininess
    // combine results
    vec3 ambient = light.ambient * thisfragcol;
    vec3 diffuse = light.diffuse * diff * thisfragcol;
    vec3 specular = light.specular * spec * thisfragcol;  // should be spec map

	vec3 clamped = clamp((light.ambient + (light.diffuse * diff) + (light.specular * spec)), 0, 1) * thisfragcol;

    return clamped; //(ambient + diffuse + specular);
}

// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    if (light.diffuse.r == 0 && light.diffuse.g == 0 && light.diffuse.b == 0) return vec3(0);    

    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 128); //material.shininess
    // attenuation
    float distance = length(light.position - fragPos);

    //float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    float attenuation = clamp(1.0 - ((distance * distance)/(light.radius * light.radius)), 0.0, 1.0);
    attenuation *= attenuation;


    
    // combine results
    vec3 ambient = light.ambient ;//* vec3(texture(material.diffuse, TexCoords));
    vec3 diffuse = light.diffuse * diff ;//* vec3(texture(material.diffuse, TexCoords));
    vec3 specular = light.specular * spec ;//* vec3(texture(material.specular, TexCoords));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    return (ambient + diffuse + specular);
}