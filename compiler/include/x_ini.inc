#if defined _INC_x_ini
	#endinput
#endif
#define _INC_x_ini

/*
 * x_ini - INI files for PawnX, a drop-in replacement for YSI's y_ini.
 *
 * The declarative loader syntax is the same as y_ini: you write an
 *
 *     INI:filename[tag](name[], value[]) { INI_Int("key", var); ... }
 *
 * callback and call INI_Load("filename"), and each "key = value" line under
 * [tag] is handed to your callback. The in-callback readers (INI_Int,
 * INI_Float, INI_Hex, INI_Bin, INI_String, INI_Bool) match a key and stop.
 *
 * Reads and the write API lean on the host: the loader uses the file natives
 * plus CallLocalFunction, and the buffered writer (INI_Open ... INI_Close)
 * maps onto open.mp's config natives (writecfg / writecfgvalue / deletecfg),
 * so a section/key write needs no plugin and no temp-file rewrite. The value
 * converters are plain Pawn.
 *
 * Existing y_ini code compiles by swapping the include:
 *   #include <YSI_Storage\y_ini>  ->  #include <x_ini>
 */

#include <file>
#include <string>
#include <float>

#if !defined MAX_INI_LINE
	#define MAX_INI_LINE (256)
#endif
#if !defined MAX_INI_TAG
	#define MAX_INI_TAG (32)
#endif
#if !defined MAX_INI_ENTRY_NAME
	#define MAX_INI_ENTRY_NAME (32)
#endif
#if !defined INI_MAX_WRITES
	#define INI_MAX_WRITES (4)
#endif

#define INI_NO_FILE (INI:-1)

// --- value converters (plain Pawn, disk-free, unit-testable) ---

// Parse an unsigned hex string, with or without a leading 0x.
stock hexstr(const string[])
{
	new i = 0;
	if (string[0] == '0' && (string[1] == 'x' || string[1] == 'X'))
		i = 2;
	new result = 0;
	for ( ; string[i] != '\0'; ++i)
	{
		new c = string[i], d;
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'f')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			d = c - 'A' + 10;
		else
			break;
		result = (result << 4) | d;
	}
	return result;
}

// Parse a binary string of 0s and 1s.
stock binstr(const string[])
{
	new result = 0;
	for (new i = 0; string[i] != '\0'; ++i)
	{
		if (string[i] == '0')
			result = result << 1;
		else if (string[i] == '1')
			result = (result << 1) | 1;
		else
			break;
	}
	return result;
}

// Parse a boolean: a number, or the words true / yes / on (case-insensitive).
stock bool:BoolStr(const string[])
{
	if (string[0] == '\0')
		return false;
	if ((string[0] >= '0' && string[0] <= '9') || string[0] == '-')
		return strval(string) != 0;
	if (!strcmp(string, "true", true) || !strcmp(string, "yes", true) || !strcmp(string, "on", true))
		return true;
	return false;
}

// Bounded copy of an unpacked string, defaulting to the destination size.
stock StrCpy(dest[], const src[], size = sizeof dest)
{
	dest[0] = '\0';
	return strcat(dest, src, size);
}

// --- declarative loader macros (same shape as y_ini) ---

#define INI_Parse(%1,%2) \
	forward @INI_%1_%2(name[], value[]); \
	@INI_%1_%2(name[], value[])

#define INI:%0[%1](%2) \
	forward @INI_%0_%1(%2); \
	@INI_%0_%1(%2)

// --- in-callback readers: match a key against the callback's name/value ---

#define INI_Int(%1,%2)    if (!strcmp((%1), name, true)) return %2 = strval(value)
#define INI_Float(%1,%2)  if (!strcmp((%1), name, true)) return _:(%2 = strfloat(value))
#define INI_Hex(%1,%2)    if (!strcmp((%1), name, true)) return %2 = hexstr(value)
#define INI_Bin(%1,%2)    if (!strcmp((%1), name, true)) return %2 = binstr(value)
#define INI_Bool(%1,%2)   if (!strcmp((%1), name, true)) return _:(%2 = BoolStr(value))
#define INI_String(%1,%2) if (!strcmp((%1), name, true)) return _:StrCpy(%2, value)

// --- buffered write API, backed by the host config natives ---

static gINIFile[INI_MAX_WRITES][MAX_INI_LINE];
static gINITag[INI_MAX_WRITES][MAX_INI_TAG];
static bool:gINIOpen[INI_MAX_WRITES];

static stock bool:INI_ValidHandle(INI:file)
{
	return 0 <= _:file < INI_MAX_WRITES && gINIOpen[_:file];
}

// Start writing to a file. Nothing is opened on disk until a value is written;
// this just reserves a handle and remembers the name.
stock INI:INI_Open(const filename[])
{
	for (new i = 0; i < INI_MAX_WRITES; ++i)
	{
		if (gINIOpen[i])
			continue;
		gINIOpen[i] = true;
		strpack(gINIFile[i], filename, sizeof gINIFile[]);
		gINITag[i][0] = '\0';
		return INI:i;
	}
	return INI_NO_FILE;
}

stock INI_Close(INI:file)
{
	if (INI_ValidHandle(file))
		gINIOpen[_:file] = false;
}

// Section header for the writes that follow.
stock bool:INI_SetTag(INI:file, const tag[])
{
	if (!INI_ValidHandle(file))
		return false;
	strpack(gINITag[_:file], tag, sizeof gINITag[]);
	return true;
}

stock bool:INI_WriteString(INI:file, const name[], const data[])
{
	if (!INI_ValidHandle(file))
		return false;
	new f[MAX_INI_LINE], t[MAX_INI_TAG];
	strunpack(f, gINIFile[_:file], sizeof f);
	strunpack(t, gINITag[_:file], sizeof t);
	return writecfg(f, t, name, data);
}

stock bool:INI_WriteInt(INI:file, const name[], data)
{
	if (!INI_ValidHandle(file))
		return false;
	new f[MAX_INI_LINE], t[MAX_INI_TAG];
	strunpack(f, gINIFile[_:file], sizeof f);
	strunpack(t, gINITag[_:file], sizeof t);
	return writecfgvalue(f, t, name, data);
}

stock bool:INI_WriteHex(INI:file, const name[], data)
{
	new out[16];
	strformat(out, sizeof out, false, "0x%x", data);
	return INI_WriteString(file, name, out);
}

stock bool:INI_WriteBin(INI:file, const name[], data)
{
	new out[36], i = 0;
	if (data == 0)
		out[i++] = '0';
	else
		for (new bit = 31; bit >= 0; --bit)
			if (data & (1 << bit) || i > 0)
				out[i++] = (data & (1 << bit)) ? '1' : '0';
	out[i] = '\0';
	return INI_WriteString(file, name, out);
}

stock bool:INI_WriteBool(INI:file, const name[], bool:data)
{
	return INI_WriteInt(file, name, data ? 1 : 0);
}

stock bool:INI_WriteFloat(INI:file, const name[], Float:data, accuracy = 6)
{
	if (accuracy < 0)
		accuracy = 0;
	if (accuracy > 9)
		accuracy = 9;
	new fmt[8] = "%.0f";
	fmt[2] = '0' + accuracy;
	new out[32];
	strformat(out, sizeof out, false, fmt, data);
	return INI_WriteString(file, name, out);
}

stock bool:INI_RemoveEntry(INI:file, const name[])
{
	if (!INI_ValidHandle(file))
		return false;
	new f[MAX_INI_LINE], t[MAX_INI_TAG];
	strunpack(f, gINIFile[_:file], sizeof f);
	strunpack(t, gINITag[_:file], sizeof t);
	return deletecfg(f, t, name);
}

stock bool:INI_DeleteTag(INI:file, const tag[])
{
	if (!INI_ValidHandle(file))
		return false;
	new f[MAX_INI_LINE];
	strunpack(f, gINIFile[_:file], sizeof f);
	return deletecfg(f, tag, "");
}

// --- loader (host: reads the file and dispatches lines to @INI_file_tag) ---

// Strip trailing and leading whitespace (including CR/LF) in place.
static stock INI_Trim(string[])
{
	new len = strlen(string);
	while (len > 0 && string[len - 1] <= ' ')
		string[--len] = '\0';
	new start = 0;
	while (string[start] != '\0' && string[start] <= ' ')
		++start;
	if (start > 0)
		strdel(string, 0, start);
}

// Reduce a path like "scriptfiles/users/bob.ini" to just "bob".
static stock INI_FileBase(const path[], base[], size)
{
	new last = -1, dot = -1;
	for (new i = 0; path[i] != '\0'; ++i)
	{
		if (path[i] == '/' || path[i] == '\\')
			last = i;
		else if (path[i] == '.')
			dot = i;
	}
	new start = last + 1, j = 0, stop = (dot > last) ? dot : strlen(path);
	while (start < stop && j < size - 1)
		base[j++] = path[start++];
	base[j] = '\0';
}

stock bool:INI_ParseFile(const fname[], const remoteFormat[], bool:bFileFirst = false, bool:bExtra = false, extra = 0, bool:bLocal = true, bool:bPassTag = false, bool:bPassFile = false)
{
	#pragma unused bExtra, extra, bLocal, bPassTag, bPassFile
	new File:f = fopen(fname, io_read);
	if (!f)
		return false;
	new base[MAX_INI_ENTRY_NAME];
	INI_FileBase(fname, base, sizeof base);
	new line[MAX_INI_LINE], tag[MAX_INI_TAG], cb[MAX_INI_LINE];
	tag[0] = '\0';
	while (fread(f, line))
	{
		INI_Trim(line);
		if (line[0] == '\0' || line[0] == ';' || line[0] == '#')
			continue;
		if (line[0] == '[')
		{
			new close = strfind(line, "]", true);
			if (close > 1)
			{
				strmid(tag, line, 1, close, sizeof tag);
				INI_Trim(tag);
			}
			continue;
		}
		new eq = strfind(line, "=", true);
		if (eq < 1)
			continue;
		new name[MAX_INI_ENTRY_NAME], value[MAX_INI_LINE];
		strmid(name, line, 0, eq, sizeof name);
		strmid(value, line, eq + 1, strlen(line), sizeof value);
		INI_Trim(name);
		INI_Trim(value);
		if (bFileFirst)
			strformat(cb, sizeof cb, false, remoteFormat, base, tag);
		else
			strformat(cb, sizeof cb, false, remoteFormat, tag, base);
		if (funcidx(cb) != -1)
			CallLocalFunction(cb, "ss", name, value);
	}
	fclose(f);
	return true;
}

// Load a file into its INI: callbacks (@INI_<file>_<tag>).
stock bool:INI_Load(const filename[], bool:bExtra = false, extra = 0, bool:bLocal = true)
{
	return INI_ParseFile(filename, "@INI_%s_%s", true, bExtra, extra, bLocal);
}

