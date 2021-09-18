imports : [
    { name : "color" type: "float3" }
]

common : [[
#define MAX_STEPS 100
#define MAX_DIST 100.0
#define SURF_DIST 0.01

    float2x2 rotate(float a)
    {
        float s = sin(a);
        float c = cos(a);

        return transpose(float2x2(c, -s, s, c));
    }

    float sd_capsule(float3 p, float3 a, float3 b, float r)
    {
        float3 ab = b - a;
        float3 ap = p - a;
        
        float t = dot(ab, ap) / dot(ab, ab);
        t = saturate(t);
        
        float3 c = a + t * ab;
        float d = length(p - c) - r;

        return d;
    }

    float sd_cylinder(float3 p, float3 a, float3 b, float r)
    {
        float3 ab = b - a;
        float3 ap = p - a;
        
        float t = dot(ab, ap) / dot(ab, ab);
        
        float3 c = a + t * ab;
        float x = length(p - c) - r;
        float y = (abs(t - 0.5) - 0.5) * length(ab);
        float e = length(max(float2(x, y), 0.0));
        float i = min(max(x, y), 0.0);

        return e + i;
    }

    float sd_torus(float3 p, float2 r)
    {
        float x = length(p.xz) - r.x;
        
        return length(float2(x, p.y)) - r.y;
    }

    float sd_box(float3 p, float3 s)
    {
        return length(max(abs(p) - s, 0.0));
    }

    float sd_subtract(float a, float b)
    {
        return max(-a, b);
    }

    float sd_add(float a, float b, float k)
    {
        float h = saturate(0.5 + 0.5 * (b - a) / k);
        return lerp(b, a, h) - k * h * (1.0 - h);
    }

    float sd_intersect(float a, float b)
    {
        return max(a, b);
    }

    float get_dist(float3 p)
    {
        float4 s = float4(0.0, 1.0, 6.0, 1.0);

        float sphere_dist = length(p - s.xyz) - s.w;
        float plane_dist = p.y;

        float cd = sd_capsule(p, float3(0.0, 1.0, 6.0), float3(1.0, 2.0, 6.0), 0.2);
        float3 tp = p;
        tp.xy = mul(tp.xy, rotate(load_time()));
        float td = sd_torus(tp - float3(0.0, 0.5, 6.0), float2(1.5, 0.5));
        float3 bp = p;
        bp -= float3(0.0, 1.0, 7.0);
        bp.xz = mul(bp.xz, rotate(load_time()));
        float bd = sd_box(bp, float3(1.0, 1.0, 1.0));
        float3 a = float3(0.0, 0.3, 3.0);
        float3 b = float3(3.0, 0.3, 7.0);
        float3 center = (a + b) / 2.0;
        float3 cylp = p;
        cylp -= center;
        cylp.xz = mul(cylp.xz, rotate(load_time()));
        cylp += center;
        float cyld = sd_cylinder(cylp, a, b, 0.2);
        float cyldb = sd_cylinder(cylp, a, b, 0.7);

        float d = sd_add(cd, plane_dist, 0.2);
        d = sd_add(d, cd, 0.2);
        d = sd_add(d, bd, 0.2);
        d = sd_add(d, cyld, 0.1);
        d = sd_add(d, sd_subtract(cyldb, td), 0.2);

        return d;
    }

    float3 get_normal(float3 p)
    {
        float d = get_dist(p);
        float2 e = float2(0.01, 0);

        float3 n = float3(d, d, d) - float3(get_dist(p - e.xyy),
                                            get_dist(p - e.yxy),
                                            get_dist(p - e.yyx));

        return normalize(n);
    }


    float ray_marching(float3 ro, float3 rd)
    {
        float d_o = 0.0;
        for (int i = 0; i < MAX_STEPS; i++) {
            float3 p = ro + rd * d_o;
            float ds = get_dist(p);
            d_o += ds;

            if (d_o > MAX_DIST || ds < SURF_DIST) 
                break;
        }

        return d_o;
    }

    float get_light(float3 p)
    {
        float3 light_pos = float3(0.0, 5.0, 2.0);
        float3 l = normalize(light_pos - p);
        float3 n = get_normal(p);
        
        float dif = saturate(dot(n, l));

        float d = ray_marching(p + n * SURF_DIST * 2.0, l);

        if (d < length(light_pos - 0))
            dif *= 0.1;

        return dif;
    }
]]

vertex_shader : {
    import_system_semantics : [ "vertex_id" ]
    exports : [
        { name: "uv" type: "float2" }
    ]

    code : [[
        static const float4 pos[3] = {
            { -1,  1,  0, 1 },
            {  3,  1,  0, 1 },
            { -1, -3,  0, 1 },
        };
    
        output.position = pos[vertex_id];

        static const float2 uvs[3] = {
            { 0, 0 },
            { 2, 0 },
            { 0, 2 },
        };

        output.uv = uvs[vertex_id];

        return output;
    ]]
}

pixel_shader : {
    exports : [
        { name : "color" type: "float4" }
    ]

    code : [[

        float2 uv = (input.position.xy / (float2)load_render_target_resolution()) * float2(1.0, -1.0) - float2(0.5, -0.5);
        uv.x *= load_render_target_resolution().x / load_render_target_resolution().y;

        float3 ro = float3(0.0, 2.0, 0.0);
        float3 rd = normalize(float3(uv.x, uv.y - 0.2, 1.0));

        float d = ray_marching(ro, rd);
        
        float3 p = ro + rd * d;
        float dif = get_light(p);

        float3 col = load_color();
        col.r += sin(d * load_time());
        col.g += cos(d * load_time());
        col.b += sin(d * load_time()) * cos(load_time());
        col *= dif;
        output.color = float4(col, 1.0);

        return output;

    ]]
}

compile : {

    variations : [
        { systems : [ "frame_system", "viewer_system" ] }
    ]
}
