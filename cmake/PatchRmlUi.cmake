# Patches RmlUi's text-transform implementation to support non-ASCII letters.
#
# Stock RmlUi implements `text-transform: uppercase` / `lowercase` as byte-wise
# ASCII comparisons (`character >= 'a' && character <= 'z'`), so accented letters
# from the Spanish locale ("Muerte instantánea" -> "MUERTE INSTANTáNEA") and any
# other Latin-1 language are left untouched and render mixed-case.
#
# This patch adds a UTF-8 aware mapping for 2-byte Latin-1 letters. Those are
# exactly the sequences 0xC3 followed by a trail byte:
#   - lowercase U+00E0..U+00FE (except ÷ U+00F7): 0xC3 A0..BE (except B7)
#   - uppercase U+00C0..U+00DE (except × U+00D7): 0xC3 80..9E (except 97)
# Case is toggled by flipping bit 0x20 of the trail byte. ASCII handling and
# everything else is left exactly as upstream.
#
# The patch is idempotent (marker check per file) and fails loudly if an anchor
# changes upstream, so bumping the RmlUi tarball never silently ships a
# half-applied patch. Follows the PatchFunchook.cmake convention.

set(_rmlui_text_cpp "${SOURCE_DIR}/Source/Core/ElementText.cpp")
set(_rmlui_string_cpp "${SOURCE_DIR}/Source/Core/StringUtilities.cpp")

foreach(_file IN ITEMS "${_rmlui_text_cpp}" "${_rmlui_string_cpp}")
    if (NOT EXISTS "${_file}")
        message(FATAL_ERROR "PatchRmlUi: missing ${_file}")
    endif ()
endforeach()

# --- 1) ElementText.cpp: BuildToken, the per-character transform used when
#        rendering `text-transform: uppercase` / `lowercase`.
file(READ "${_rmlui_text_cpp}" _text_content)

if (_text_content MATCHES "DUSKLIGHT_TEXT_TRANSFORM_UTF8")
    message(STATUS "PatchRmlUi: ElementText.cpp already patched")
else ()
    set(_anchor [=[
			if (text_transformation == Style::TextTransform::Uppercase)
			{
				if (character >= 'a' && character <= 'z')
					character += ('A' - 'a');
			}
			else if (text_transformation == Style::TextTransform::Lowercase)
			{
				if (character >= 'A' && character <= 'Z')
					character -= ('A' - 'a');
			}
]=])

    set(_replacement [=[
			if (text_transformation == Style::TextTransform::Uppercase)
			{
				if (character >= 'a' && character <= 'z')
					character += ('A' - 'a');
				// DUSKLIGHT_TEXT_TRANSFORM_UTF8: 2-byte Latin-1 letters
				// (0xC3 0xA0..0xBE except 0xB7) -> uppercase.
				else if (static_cast<unsigned char>(character) == 0xC3 && token_begin + 1 < string_end)
				{
					const unsigned char trail = static_cast<unsigned char>(token_begin[1]);
					if (trail >= 0xA0 && trail <= 0xBE && trail != 0xB7)
					{
						token += character;
						token += static_cast<char>(trail & ~0x20);
						++token_begin;
						++token_begin;
						continue;
					}
				}
			}
			else if (text_transformation == Style::TextTransform::Lowercase)
			{
				if (character >= 'A' && character <= 'Z')
					character -= ('A' - 'a');
				// DUSKLIGHT_TEXT_TRANSFORM_UTF8: 2-byte Latin-1 letters
				// (0xC3 0x80..0x9E except 0x97) -> lowercase.
				else if (static_cast<unsigned char>(character) == 0xC3 && token_begin + 1 < string_end)
				{
					const unsigned char trail = static_cast<unsigned char>(token_begin[1]);
					if (trail >= 0x80 && trail <= 0x9E && trail != 0x97)
					{
						token += character;
						token += static_cast<char>(trail | 0x20);
						++token_begin;
						++token_begin;
						continue;
					}
				}
			}
]=])

    string(FIND "${_text_content}" "${_anchor}" _pos)
    if (_pos EQUAL -1)
        message(FATAL_ERROR "PatchRmlUi: BuildToken anchor not found; RmlUi source changed and cmake/PatchRmlUi.cmake needs updating")
    endif ()
    string(REPLACE "${_anchor}" "${_replacement}" _text_content "${_text_content}")
    file(WRITE "${_rmlui_text_cpp}" "${_text_content}")
    message(STATUS "PatchRmlUi: patched ElementText.cpp (UTF-8 text-transform)")
endif ()

# --- 2) StringUtilities.cpp: ToUpper, used by data-binding formatters so bound
#        strings transform consistently with rendered text. ToLower is left
#        untouched on purpose: it is used for ASCII CSS keyword parsing.
file(READ "${_rmlui_string_cpp}" _string_content)

if (_string_content MATCHES "DUSKLIGHT_TEXT_TRANSFORM_UTF8")
    message(STATUS "PatchRmlUi: StringUtilities.cpp already patched")
    return()
endif ()

set(_string_anchor [=[String StringUtilities::ToUpper(String string)
{
	std::transform(string.begin(), string.end(), string.begin(), [](char c) {
		if (c >= 'a' && c <= 'z')
			c -= char('a' - 'A');
		return c;
	});
	return string;
}
]=])

set(_string_replacement [=[String StringUtilities::ToUpper(String string)
{
	// DUSKLIGHT_TEXT_TRANSFORM_UTF8: ASCII mapping plus 2-byte Latin-1 letters.
	for (size_t i = 0; i < string.size(); ++i)
	{
		char c = string[i];
		if (c >= 'a' && c <= 'z')
		{
			c -= char('a' - 'A');
			string[i] = c;
		}
		else if (static_cast<unsigned char>(c) == 0xC3 && i + 1 < string.size())
		{
			const unsigned char trail = static_cast<unsigned char>(string[i + 1]);
			if (trail >= 0xA0 && trail <= 0xBE && trail != 0xB7)
			{
				string[++i] = static_cast<char>(trail & ~0x20);
			}
		}
	}
	return string;
}
]=])

string(FIND "${_string_content}" "${_string_anchor}" _pos)
if (_pos EQUAL -1)
    message(FATAL_ERROR "PatchRmlUi: StringUtilities::ToUpper anchor not found; RmlUi source changed and cmake/PatchRmlUi.cmake needs updating")
endif ()
string(REPLACE "${_string_anchor}" "${_string_replacement}" _string_content "${_string_content}")
file(WRITE "${_rmlui_string_cpp}" "${_string_content}")

message(STATUS "PatchRmlUi: patched StringUtilities.cpp (UTF-8 ToUpper)")
