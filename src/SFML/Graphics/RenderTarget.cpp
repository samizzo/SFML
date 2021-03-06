////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2015 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Graphics/DefaultShaders.hpp>
#include <SFML/System/Err.hpp>
#include <cassert>
#include <iostream>

namespace
{
    // Convert an sf::BlendMode::Factor constant to the corresponding OpenGL constant.
    sf::Uint32 factorToGlConstant(sf::BlendMode::Factor blendFactor)
    {
        switch (blendFactor)
        {
            case sf::BlendMode::Zero:             return GL_ZERO;
            case sf::BlendMode::One:              return GL_ONE;
            case sf::BlendMode::SrcColor:         return GL_SRC_COLOR;
            case sf::BlendMode::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
            case sf::BlendMode::DstColor:         return GL_DST_COLOR;
            case sf::BlendMode::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
            case sf::BlendMode::SrcAlpha:         return GL_SRC_ALPHA;
            case sf::BlendMode::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
            case sf::BlendMode::DstAlpha:         return GL_DST_ALPHA;
            case sf::BlendMode::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        }

        sf::err() << "Invalid value for sf::BlendMode::Factor! Fallback to sf::BlendMode::Zero." << std::endl;
        assert(false);
        return GL_ZERO;
    }


    // Convert an sf::BlendMode::BlendEquation constant to the corresponding OpenGL constant.
    sf::Uint32 equationToGlConstant(sf::BlendMode::Equation blendEquation)
    {
        switch (blendEquation)
        {
            case sf::BlendMode::Add:             return GLEXT_GL_FUNC_ADD;
            case sf::BlendMode::Subtract:        return GLEXT_GL_FUNC_SUBTRACT;
            case sf::BlendMode::ReverseSubtract: return GLEXT_GL_FUNC_REVERSE_SUBTRACT;
        }

        sf::err() << "Invalid value for sf::BlendMode::Equation! Fallback to sf::BlendMode::Add." << std::endl;
        assert(false);
        return GLEXT_GL_FUNC_ADD;
    }
}


namespace sf
{

// This is static so that all RenderTargets use the same cache. This isn't thread-safe but
// we don't render from multiple threads so it's ok. This must be static because we have
// multiple RenderTargets that all render with the same context, and if they don't share a
// a cache, the cache is not in sync with the OpenGL state, and rendering can occur with,
// for example, the wrong vertex array set. When using a separate context for each RenderTarget
// this isn't a problem, but Moonman shares the main context for all RenderTargets.
RenderTarget::StatesCache RenderTarget::s_cache;

////////////////////////////////////////////////////////////
RenderTarget::RenderTarget() :
m_defaultView(),
m_view       ()
{
    s_cache.glStatesSet = false;
}


////////////////////////////////////////////////////////////
RenderTarget::~RenderTarget()
{
	glCheck(glDeleteBuffers(1, &m_spriteIndexVBO));
	glCheck(glDeleteBuffers(1, &m_spriteVertexVBO));
}


////////////////////////////////////////////////////////////
void RenderTarget::clear(const Color& color)
{
    if (activate(true))
    {
        // Unbind texture to fix RenderTexture preventing clear
        applyTexture(RenderStates());

        glCheck(glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
        glCheck(glClear(GL_COLOR_BUFFER_BIT));
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::setView(const View& view)
{
    m_view = view;
    s_cache.viewChanged = true;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getView() const
{
    return m_view;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getDefaultView() const
{
    return m_defaultView;
}


////////////////////////////////////////////////////////////
IntRect RenderTarget::getViewport(const View& view) const
{
    float width  = static_cast<float>(getSize().x);
    float height = static_cast<float>(getSize().y);
    const FloatRect& viewport = view.getViewport();

    return IntRect(static_cast<int>(0.5f + width  * viewport.left),
                   static_cast<int>(0.5f + height * viewport.top),
                   static_cast<int>(0.5f + width  * viewport.width),
                   static_cast<int>(0.5f + height * viewport.height));
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(const Vector2i& point) const
{
    return mapPixelToCoords(point, getView());
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(const Vector2i& point, const View& view) const
{
    // First, convert from viewport coordinates to homogeneous coordinates
    Vector2f normalized;
    IntRect viewport = getViewport(view);
    normalized.x = -1.f + 2.f * (point.x - viewport.left) / viewport.width;
    normalized.y =  1.f - 2.f * (point.y - viewport.top)  / viewport.height;

    // Then transform by the inverse of the view matrix
    return view.getInverseTransform().transformPoint(normalized);
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(const Vector2f& point) const
{
    return mapCoordsToPixel(point, getView());
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(const Vector2f& point, const View& view) const
{
    // First, transform the point by the view matrix
    Vector2f normalized = view.getTransform().transformPoint(point);

    // Then convert to viewport coordinates
    Vector2i pixel;
    IntRect viewport = getViewport(view);
    pixel.x = static_cast<int>(( normalized.x + 1.f) / 2.f * viewport.width  + viewport.left);
    pixel.y = static_cast<int>((-normalized.y + 1.f) / 2.f * viewport.height + viewport.top);

    return pixel;
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Drawable& drawable, const RenderStates& states)
{
    drawable.draw(*this, states);
}

void RenderTarget::drawAdvanced(const Drawable& drawable, const RenderStates& states)
{
    drawable.drawAdvanced(*this, states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Vertex* vertices, std::size_t vertexCount,
                        PrimitiveType type, const RenderStates& states)
{
    // Nothing to draw?
    if (!vertices || (vertexCount == 0))
        return;

    // GL_QUADS is unavailable on OpenGL ES
    #ifdef SFML_OPENGL_ES
        if (type == Quads)
        {
            err() << "sf::Quads primitive type is not supported on OpenGL ES platforms, drawing skipped" << std::endl;
            return;
        }
        #define GL_QUADS 0
    #endif

    if (activate(true))
    {
        // First set the persistent OpenGL states if it's the very first call
        if (!s_cache.glStatesSet)
            resetGLStates();

        // Check if the vertex count is low enough so that we can pre-transform them
        bool useVertexCache = (vertexCount <= StatesCache::VertexCacheSize) && !states.useVBO;
        if (useVertexCache)
        {
            // Pre-transform the vertices and store them into the vertex cache
            for (std::size_t i = 0; i < vertexCount; ++i)
            {
                Vertex& vertex = s_cache.vertexCache[i];
                vertex.position = states.transform * vertices[i].position;
                vertex.color = vertices[i].color;
                vertex.texCoords = vertices[i].texCoords;
            }

            // Since vertices are transformed, we must use an identity transform to render them
            if (!s_cache.useVertexCache)
                applyTransform(Transform::Identity);
        }
        else
        {
            applyTransform(states.transform);
        }

        // Apply the view
        if (s_cache.viewChanged)
            applyCurrentView();

        // Apply the blend mode
        if (states.blendMode != s_cache.lastBlendMode)
            applyBlendMode(states.blendMode);

        // Apply the shader
        Shader::DefaultShaderType defaultShaderType = states.texture == NULL ? Shader::Untextured : Shader::Textured;
        Shader* shader = states.shader ? (Shader*)states.shader : Shader::getDefaultShader(defaultShaderType);

        if (!states.shader && defaultShaderType == Shader::Textured)
        {
            // If there is no shader set, this means SFML would have used the fixed function
			// pipeline to render without a shader. This implies a single texture is used, because
			// SFML doesn't use texture combiners so it's useless to specify multiple textures.
			// We therefore need to set u_tex in our fixed function default shader to the
			// required texture.
            int texLocation = Shader::getDefaultShaderTextureUniformLocation();
			shader->setUniform(texLocation, sf::Shader::CurrentTextureType());
        }

		// Apply the texture
		Uint64 textureId = states.texture ? states.texture->m_cacheId : 0;
		if (textureId != s_cache.lastTextureId || states.textureTransform != NULL)
			applyTexture(states);

        bool setColour = false;
        unsigned int program = shader->getNativeHandle();

		// If the shader has changed, or it hasn't but the last time we didn't bind textures, then we need to setup the shader again.
		if (program != s_cache.lastProgram || !s_cache.lastProgramBoundTextures)
        {
            setColour = true;
			applyShader(shader);
        }

        // Apply the color.
		Color color = states.useColor ? states.color : Color::White;
        if (color != s_cache.lastColor || setColour)
        {
            Glsl::Vec4 colorVec(color);
			int colorLocation = shader->getColorLocation();
            // HACK: We don't set this via the Shader class because we don't want to bind again.
            glCheck(GLEXT_glUniform4f(colorLocation, colorVec.x, colorVec.y, colorVec.z, colorVec.w));
            s_cache.lastColor = color;
        }

        // If we pre-transform the vertices, we must use our internal vertex cache
        if (useVertexCache)
        {
            // ... and if we already used it previously, we don't need to set the pointers again
            if (!s_cache.useVertexCache)
                vertices = s_cache.vertexCache;
            else
                vertices = NULL;
        }

#if defined(SFML_DEBUG)
		// There should always be a shader bound, since we fall back to a default shader.
		GLhandleARB currentShaderHandle = glGetHandleARB(GL_PROGRAM_OBJECT_ARB);
      #if defined(SFML_SYSTEM_MACOS)
      assert(currentShaderHandle != (void*)-1);
      #else
		assert(currentShaderHandle != -1);
      #endif
#endif

		if (s_cache.lastUsedVBO && !states.useVBO)
		{
			// No longer using a VBO so make sure there's nothing bound.
			glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
			glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
		}

		if (states.useVBO)
		{
			if (!s_cache.lastUsedVBO)
			{
				// Specify the vertices.
				glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_spriteVertexVBO));
				glCheck(glVertexPointer(2, GL_FLOAT, sizeof(Vertex), (void*)0));
				glCheck(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)8));
				glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (void*)12));

				// Specify the indices.
				glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_spriteIndexVBO));
			}

			glCheck(glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, (void*)0));
		}
		else if (vertices)
		{
			// Setup the pointers to the vertices' components
			const char* data = reinterpret_cast<const char*>(vertices);
			glCheck(glVertexPointer(2, GL_FLOAT, sizeof(Vertex), data + 0));
			glCheck(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), data + 8));
			glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), data + 12));
		}

		s_cache.lastUsedVBO = states.useVBO;

		if (!states.useVBO)
		{
			// Find the OpenGL primitive type
			static const GLenum modes[] = {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES,
										   GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_QUADS};
			GLenum mode = modes[type];

			// Draw the primitives
			glCheck(glDrawArrays(mode, 0, vertexCount));
		}

        // If the texture we used to draw belonged to a RenderTexture, then forcibly unbind that texture.
        // This prevents a bug where some drivers do not clear RenderTextures properly.
        if (states.texture && states.texture->m_fboAttachment)
            applyTexture(RenderStates());

        // Update the cache
        s_cache.useVertexCache = useVertexCache;
    }
}


void RenderTarget::drawAdvanced(const Vertex* vertices, std::size_t vertexCount,
    PrimitiveType type, const RenderStates& states)
{
    // Nothing to draw?
    if (!vertices || (vertexCount == 0))
        return;

	// There must be a shader bound for this version to be used.
	assert(states.shader != NULL);
#if defined(SFML_DEBUG)
	GLhandleARB currentShaderHandle = glGetHandleARB(GL_PROGRAM_OBJECT_ARB);
	assert(currentShaderHandle != 0);
#endif

    // GL_QUADS is unavailable on OpenGL ES
#ifdef SFML_OPENGL_ES
    if (type == Quads)
    {
        err() << "sf::Quads primitive type is not supported on OpenGL ES platforms, drawing skipped" << std::endl;
        return;
    }
#define GL_QUADS 0
#endif

    // First set the persistent OpenGL states if it's the very first call
    if (!s_cache.glStatesSet)
        resetGLStates();

    // Check if the vertex count is low enough so that we can pre-transform them
    bool useVertexCache = (vertexCount <= StatesCache::VertexCacheSize) && !states.useVBO;
    if (useVertexCache)
    {
        // Pre-transform the vertices and store them into the vertex cache
        for (std::size_t i = 0; i < vertexCount; ++i)
        {
            Vertex& vertex = s_cache.vertexCache[i];
            vertex.position = states.transform * vertices[i].position;
            vertex.color = vertices[i].color;
            vertex.texCoords = vertices[i].texCoords;
        }

        // Since vertices are transformed, we must use an identity transform to render them
        if (!s_cache.useVertexCache)
            applyTransform(Transform::Identity);
    }
    else
    {
        applyTransform(states.transform);
    }

    // Apply the view
    if (s_cache.viewChanged)
        applyCurrentView();

    // Apply the blend mode
    if (states.blendMode != s_cache.lastBlendMode)
        applyBlendMode(states.blendMode);

    // Apply the texture
    Uint64 textureId = states.texture ? states.texture->m_cacheId : 0;
    if (textureId != s_cache.lastTextureId || states.textureTransform)
        applyTexture(states);

	// Apply the color.
	Color color = states.useColor ? states.color : Color::White;
	// HACK: Always set the colour for now until we iron out all the bugs.
	// This should only change the colour if it has changed, or if the shader has changed.
	//if (color != s_cache.lastColor)
	{
		Glsl::Vec4 colorVec(color);
		int colorLocation = states.shader->getColorLocation();
		// HACK: We don't set this via the Shader class because we don't want to bind again.
		glCheck(GLEXT_glUniform4f(colorLocation, colorVec.x, colorVec.y, colorVec.z, colorVec.w));
		s_cache.lastColor = color;
	}

    // If we pre-transform the vertices, we must use our internal vertex cache
    if (useVertexCache)
    {
        // ... and if we already used it previously, we don't need to set the pointers again
        if (!s_cache.useVertexCache)
            vertices = s_cache.vertexCache;
        else
            vertices = NULL;
    }

	if (s_cache.lastUsedVBO && !states.useVBO)
	{
		// No longer using a VBO so make sure there's nothing bound.
		glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
		glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
	}

	if (states.useVBO)
	{
		if (!s_cache.lastUsedVBO)
		{
			// Specify the vertices.
			glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_spriteVertexVBO));
			glCheck(glVertexPointer(2, GL_FLOAT, sizeof(Vertex), (void*)0));
			glCheck(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)8));
			glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (void*)12));

			// Specify the indices.
			glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_spriteIndexVBO));
		}

		glCheck(glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_SHORT, (void*)0));
	}
	else if (vertices)
	{
		// Setup the pointers to the vertices' components
		const char* data = reinterpret_cast<const char*>(vertices);
		glCheck(glVertexPointer(2, GL_FLOAT, sizeof(Vertex), data + 0));
		glCheck(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(Vertex), data + 8));
		glCheck(glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), data + 12));
	}

	s_cache.lastUsedVBO = states.useVBO;

	if (!states.useVBO)
	{
		// Find the OpenGL primitive type
		static const GLenum modes[] = { GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES,
			GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN, GL_QUADS };
		GLenum mode = modes[type];

		// Draw the primitives
		glCheck(glDrawArrays(mode, 0, vertexCount));
	}

    // If the texture we used to draw belonged to a RenderTexture, then forcibly unbind that texture.
    // This prevents a bug where some drivers do not clear RenderTextures properly.
    if (states.texture && states.texture->m_fboAttachment)
        applyTexture(RenderStates());

    // Update the cache
    s_cache.useVertexCache = useVertexCache;
}

////////////////////////////////////////////////////////////
void RenderTarget::pushGLStates()
{
    if (activate(true))
    {
        #ifdef SFML_DEBUG
            // make sure that the user didn't leave an unchecked OpenGL error
            GLenum error = glGetError();
            if (error != GL_NO_ERROR)
            {
                err() << "OpenGL error (" << error << ") detected in user code, "
                      << "you should check for errors with glGetError()"
                      << std::endl;
            }
        #endif

        #ifndef SFML_OPENGL_ES
            glCheck(glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS));
            glCheck(glPushAttrib(GL_ALL_ATTRIB_BITS));
        #endif
        glCheck(glMatrixMode(GL_MODELVIEW));
        glCheck(glPushMatrix());
        glCheck(glMatrixMode(GL_PROJECTION));
        glCheck(glPushMatrix());
        glCheck(glMatrixMode(GL_TEXTURE));
        glCheck(glPushMatrix());
    }

    resetGLStates();
}


////////////////////////////////////////////////////////////
void RenderTarget::popGLStates()
{
    if (activate(true))
    {
        glCheck(glMatrixMode(GL_PROJECTION));
        glCheck(glPopMatrix());
        glCheck(glMatrixMode(GL_MODELVIEW));
        glCheck(glPopMatrix());
        glCheck(glMatrixMode(GL_TEXTURE));
        glCheck(glPopMatrix());
        #ifndef SFML_OPENGL_ES
            glCheck(glPopClientAttrib());
            glCheck(glPopAttrib());
        #endif
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::resetGLStates(bool applyOnly /*= false*/)
{
    // Check here to make sure a context change does not happen after activate(true)
    bool shaderAvailable = Shader::isAvailable();

    if (activate(true))
    {
        // Make sure that extensions are initialized
        priv::ensureExtensionsInit();

		if (!applyOnly)
		{
			// Make sure that the texture unit which is active is the number 0
			if (GLEXT_multitexture)
			{
				glCheck(GLEXT_glClientActiveTexture(GLEXT_GL_TEXTURE0));
				glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0));
			}

			// Define the default OpenGL states
			glCheck(glDisable(GL_CULL_FACE));
			glCheck(glDisable(GL_LIGHTING));
			glCheck(glDisable(GL_DEPTH_TEST));
			glCheck(glDisable(GL_ALPHA_TEST));
			glCheck(glEnable(GL_TEXTURE_2D));
			glCheck(glEnable(GL_BLEND));
			glCheck(glMatrixMode(GL_MODELVIEW));
			glCheck(glEnableClientState(GL_VERTEX_ARRAY));
			glCheck(glEnableClientState(GL_COLOR_ARRAY));
			glCheck(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
			s_cache.glStatesSet = true;
		}

        // Apply the default SFML states
        applyBlendMode(BlendAlpha);
        applyTransform(Transform::Identity);
        applyTexture(RenderStates());
        if (shaderAvailable)
            applyShader(NULL);

		if (!applyOnly)
		{
			// Make sure no VBO is bound by default.
			glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
			glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

			s_cache.useVertexCache = false;
			s_cache.lastUsedVBO = false;

			// Set the default view
			setView(getView());
		}
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::initialize()
{
    // Setup the default and current views
    m_defaultView.reset(FloatRect(0, 0, static_cast<float>(getSize().x), static_cast<float>(getSize().y)));
    m_view = m_defaultView;

    // Set GL states only on first draw, so that we don't pollute user's states
    s_cache.glStatesSet = false;

	Vertex vertices[4];
	vertices[0].position = Vector2f(0, 0);
	vertices[1].position = Vector2f(0, 1.0f);
	vertices[2].position = Vector2f(1.0f, 0);
	vertices[3].position = Vector2f(1.0f, 1.0f);

	vertices[0].texCoords = Vector2f(0.0f, 0.0f);
	vertices[1].texCoords = Vector2f(0.0f, 1.0f);
	vertices[2].texCoords = Vector2f(1.0f, 0.0f);
	vertices[3].texCoords = Vector2f(1.0f, 1.0f);

	vertices[0].color = Color::White;
	vertices[1].color = Color::White;
	vertices[2].color = Color::White;
	vertices[3].color = Color::White;

	Uint16 indices[4] = { 0, 1, 2, 3 };

	// Vertices
	glCheck(glGenBuffers(1, &m_spriteVertexVBO));
	glCheck(glBindBuffer(GL_ARRAY_BUFFER, m_spriteVertexVBO));
	glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0].position.x, GL_STATIC_DRAW));

	// Indices
	glCheck(glGenBuffers(1, &m_spriteIndexVBO));
	glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_spriteIndexVBO));
	glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));

	glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));
	glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}


////////////////////////////////////////////////////////////
void RenderTarget::applyCurrentView()
{
    // Set the viewport
    IntRect viewport = getViewport(m_view);
    int top = getSize().y - (viewport.top + viewport.height);
    glCheck(glViewport(viewport.left, top, viewport.width, viewport.height));

    // Set the projection matrix
    glCheck(glMatrixMode(GL_PROJECTION));
    glCheck(glLoadMatrixf(m_view.getTransform().getMatrix()));

    // Go back to model-view mode
    glCheck(glMatrixMode(GL_MODELVIEW));

    s_cache.viewChanged = false;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyBlendMode(const BlendMode& mode)
{
    // Apply the blend mode, falling back to the non-separate versions if necessary
    if (GLEXT_blend_func_separate)
    {
        glCheck(GLEXT_glBlendFuncSeparate(
            factorToGlConstant(mode.colorSrcFactor), factorToGlConstant(mode.colorDstFactor),
            factorToGlConstant(mode.alphaSrcFactor), factorToGlConstant(mode.alphaDstFactor)));
    }
    else
    {
        glCheck(glBlendFunc(
            factorToGlConstant(mode.colorSrcFactor),
            factorToGlConstant(mode.colorDstFactor)));
    }

    if (GLEXT_blend_minmax && GLEXT_blend_subtract)
    {
        if (GLEXT_blend_equation_separate)
        {
            glCheck(GLEXT_glBlendEquationSeparate(
                equationToGlConstant(mode.colorEquation),
                equationToGlConstant(mode.alphaEquation)));
        }
        else
        {
            glCheck(GLEXT_glBlendEquation(equationToGlConstant(mode.colorEquation)));
        }
    }
    else if ((mode.colorEquation != BlendMode::Add) || (mode.alphaEquation != BlendMode::Add))
    {
        static bool warned = false;

        if (!warned)
        {
            err() << "OpenGL extension EXT_blend_minmax and/or EXT_blend_subtract unavailable" << std::endl;
            err() << "Selecting a blend equation not possible" << std::endl;
            err() << "Ensure that hardware acceleration is enabled if available" << std::endl;

            warned = true;
        }
    }

    s_cache.lastBlendMode = mode;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyTransform(const Transform& transform)
{
    // No need to call glMatrixMode(GL_MODELVIEW), it is always the
    // current mode (for optimization purpose, since it's the most used)
    glCheck(glLoadMatrixf(transform.getMatrix()));
}


////////////////////////////////////////////////////////////
void RenderTarget::applyTexture(const RenderStates& states)
{
	const Texture* texture = states.texture;
	const Transform* textureTransform = states.textureTransform;
	Texture::bind(texture, Texture::Pixels, textureTransform);

    s_cache.lastTextureId = texture ? texture->m_cacheId : 0;
}


void RenderTarget::setLastProgram(unsigned int program, bool boundTextures)
{
	s_cache.lastProgram = program;
	s_cache.lastProgramBoundTextures = boundTextures;
}

////////////////////////////////////////////////////////////
void RenderTarget::applyShader(const Shader* shader)
{
    Shader::bind(shader);
	s_cache.lastProgram = shader ? shader->getNativeHandle() : 0;
	s_cache.lastProgramBoundTextures = shader != NULL;
}

} // namespace sf


////////////////////////////////////////////////////////////
// Render states caching strategies
//
// * View
//   If SetView was called since last draw, the projection
//   matrix is updated. We don't need more, the view doesn't
//   change frequently.
//
// * Transform
//   The transform matrix is usually expensive because each
//   entity will most likely use a different transform. This can
//   lead, in worst case, to changing it every 4 vertices.
//   To avoid that, when the vertex count is low enough, we
//   pre-transform them and therefore use an identity transform
//   to render them.
//
// * Blending mode
//   Since it overloads the == operator, we can easily check
//   whether any of the 6 blending components changed and,
//   thus, whether we need to update the blend mode.
//
// * Texture
//   Storing the pointer or OpenGL ID of the last used texture
//   is not enough; if the sf::Texture instance is destroyed,
//   both the pointer and the OpenGL ID might be recycled in
//   a new texture instance. We need to use our own unique
//   identifier system to ensure consistent caching.
//
// * Shader
//   Shaders are very hard to optimize, because they have
//   parameters that can be hard (if not impossible) to track,
//   like matrices or textures. The only optimization that we
//   do is that we avoid setting a null shader if there was
//   already none for the previous draw.
//
////////////////////////////////////////////////////////////
