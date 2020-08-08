/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2020                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include "fragment.glsl"
#include "PowerScaling/powerScaling_fs.hglsl"

in vec4 vs_position;
in vec2 vs_textureCoords;
in vec3 vs_normal;

uniform float time;
uniform sampler2D colorTexture;
uniform float opacity;
uniform bool mirrorTexture;

uniform int projection;

#define M_PI 3.1415926535897932384626433832795

Fragment getFragment() {
    vec2 texCoord = vs_textureCoords;

    Fragment frag;
    if (mirrorTexture) {
        texCoord.x = 1.0 - texCoord.x;
    }

    if (projection == 1) {

        float rtwo = sqrt(2);
        float r =  1;//1/(2*rtwo);

        float theta = asin(texCoord.y/(r  * rtwo)); //1=0.7853981634
        float lat = asin((2*theta + sin(2*theta))/M_PI); //1=0.9584643664
        float lon = M_PI * texCoord.x/(2 * r * rtwo * cos(theta)); //1=1.5707963268

        texCoord.x = lon / 1.570796;
        texCoord.y = lat / 0.958464;
        // texCoord.x = lon;
        // texCoord.y = lat;
    }

    frag.color = texture(colorTexture, texCoord);
    frag.color.a *= opacity;
    frag.depth = vs_position.w;

    // G-Buffer
    frag.gPosition = vs_position;
    frag.gNormal = vec4(vs_normal, 1.0);

    return frag;
}
