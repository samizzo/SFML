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
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <cstdlib>

//#define TRANSFORM_VERTS


namespace sf
{
////////////////////////////////////////////////////////////
Sprite::Sprite() :
m_texture(NULL),
m_textureRect(),
m_color(Color::White)
{
}


////////////////////////////////////////////////////////////
Sprite::Sprite(const Texture& texture) :
m_texture    (NULL),
m_textureRect(),
m_color(Color::White)
{
    setTexture(texture);
}


////////////////////////////////////////////////////////////
Sprite::Sprite(const Texture& texture, const IntRect& rectangle) :
m_texture    (NULL),
m_textureRect(),
m_color(Color::White)
{
    setTexture(texture);
    setTextureRect(rectangle);
}


////////////////////////////////////////////////////////////
void Sprite::setTexture(const Texture& texture, bool resetRect)
{
	const Texture* oldTexture = m_texture;

	// Assign the new texture
	m_texture = &texture;

    // Recompute the texture area if requested, or if there was no valid texture & rect before
    if (resetRect || (!oldTexture && (m_textureRect == sf::IntRect())))
        setTextureRect(IntRect(0, 0, texture.getSize().x, texture.getSize().y));
}


////////////////////////////////////////////////////////////
void Sprite::setTextureRect(const IntRect& rectangle)
{
    if (rectangle != m_textureRect)
    {
        m_textureRect = rectangle;
        updatePositions();
        updateTexCoords();
    }
}


////////////////////////////////////////////////////////////
void Sprite::setColor(const Color& color)
{
	// Vertices are always white.
	m_vertices[0].color = Color::White;
	m_vertices[1].color = Color::White;
	m_vertices[2].color = Color::White;
	m_vertices[3].color = Color::White;
	m_color = color;
}


////////////////////////////////////////////////////////////
const Texture* Sprite::getTexture() const
{
    return m_texture;
}


////////////////////////////////////////////////////////////
const IntRect& Sprite::getTextureRect() const
{
    return m_textureRect;
}


////////////////////////////////////////////////////////////
const Color& Sprite::getColor() const
{
#if defined(TRANSFORM_VERTS)
    return m_vertices[0].color;
#else
	return m_color;
#endif
}


////////////////////////////////////////////////////////////
FloatRect Sprite::getLocalBounds() const
{
    float width = static_cast<float>(std::abs(m_textureRect.width));
    float height = static_cast<float>(std::abs(m_textureRect.height));

    return FloatRect(0.f, 0.f, width, height);
}


////////////////////////////////////////////////////////////
FloatRect Sprite::getGlobalBounds() const
{
    return getTransform().transformRect(getLocalBounds());
}


////////////////////////////////////////////////////////////
void Sprite::draw(RenderTarget& target, RenderStates states) const
{
    if (m_texture)
    {
#if defined(TRANSFORM_VERTS)
        states.transform *= getTransform();
		states.useVBO = false;
#else
		states.transform *= getTransform() * m_vertexTransform;
		states.textureTransform = &m_textureTransform;
		states.color = m_color;
		states.useColor = true;
		states.useVBO = true;
#endif
        states.texture = m_texture;
        target.draw(m_vertices, 4, TrianglesStrip, states);
    }
}

////////////////////////////////////////////////////////////
void Sprite::drawAdvanced(RenderTarget& target, RenderStates states) const
{
    if (m_texture)
    {
#if defined(TRANSFORM_VERTS)
        states.transform *= getTransform();
		states.useVBO = false;
#else
		states.transform *= getTransform() * m_vertexTransform;
		states.textureTransform = &m_textureTransform;
		states.color = m_color;
		states.useColor = true;
		states.useVBO = true;
#endif
        states.texture = m_texture;
        target.drawAdvanced(m_vertices, 4, TrianglesStrip, states);
    }
}

////////////////////////////////////////////////////////////
void Sprite::updatePositions()
{
    FloatRect bounds = getLocalBounds();

#if defined(TRANSFORM_VERTS)
    m_vertices[0].position = Vector2f(0, 0);
    m_vertices[1].position = Vector2f(0, bounds.height);
    m_vertices[2].position = Vector2f(bounds.width, 0);
    m_vertices[3].position = Vector2f(bounds.width, bounds.height);
#else
	m_vertices[0].position = Vector2f(0, 0);
	m_vertices[1].position = Vector2f(0, 1.0f);
	m_vertices[2].position = Vector2f(1.0f, 0);
	m_vertices[3].position = Vector2f(1.0f, 1.0f);
	m_vertexTransform = Transform(
		bounds.width, 0.0f, 0.0f,
		0.0f, bounds.height, 0.0f,
		0.0f, 0.0f, 1.0f
		);
#endif
}


////////////////////////////////////////////////////////////
void Sprite::updateTexCoords()
{
    float left   = static_cast<float>(m_textureRect.left);
    float right  = left + m_textureRect.width;
    float top    = static_cast<float>(m_textureRect.top);
    float bottom = top + m_textureRect.height;

#if defined(TRANSFORM_VERTS)
	m_vertices[0].texCoords = Vector2f(left, top);
	m_vertices[1].texCoords = Vector2f(left, bottom);
	m_vertices[2].texCoords = Vector2f(right, top);
	m_vertices[3].texCoords = Vector2f(right, bottom);
#else
	m_vertices[0].texCoords = Vector2f(0.0f, 0.0f);
	m_vertices[1].texCoords = Vector2f(0.0f, 1.0f);
	m_vertices[2].texCoords = Vector2f(1.0f, 0.0f);
	m_vertices[3].texCoords = Vector2f(1.0f, 1.0f);

	Vector2u actualSize = m_texture->getActualSize();
	float xscale = (right - left) / (float)actualSize.x;
	float yscale = (bottom - top) / (float)actualSize.y;
	float xorigin = left / (float)actualSize.x;
	float yorigin = top / (float)actualSize.y;

	if (m_texture->m_pixelsFlipped)
	{
		yscale *= -1.0f;
		Vector2u size = m_texture->getSize();
		yorigin += size.y / (float)actualSize.y;
	}

	m_textureTransform = Transform(
		xscale, 0.0f, xorigin,
		0.0f, yscale, yorigin,
		0.0f, 0.0f, 1.0f
	);
#endif
}

} // namespace sf
