/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2021                                                               *
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

uniform sampler2D prevTexture;
uniform sampler2D nextTexture;
uniform float blendFactor;

in vec2 texCoord;
//out vec4 fragColor;

Fragment getFragment() {
    vec4 texel0 = texture2D(prevTexture, texCoord);
    vec4 texel1 = texture2D(nextTexture, texCoord);
    //fragColor = mix(texel0, texel1, blendFactor);

    Fragment frag;
	frag.color = mix(texel0, texel1, blendFactor);
    //frag.color = vec4(blendFactor, 1-blendFactor, 0.0, 1.0);
    // Place stars at back to begin with. 
    //frag.depth = 0;
    //frag.gNormal = vec4(0.0, 0.0, 0.0, 1.0);
    //frag.blend = BLEND_MODE_NORMAL;

    return frag;
}
