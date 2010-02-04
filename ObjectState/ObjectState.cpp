/******************************************************************************
/ ObjectState.cpp
/
/ Copyright (c) 2010 Tim Payne (SWS)
/ http://www.standingwaterstudios.com/reaper
/
/ Permission is hereby granted, free of charge, to any person obtaining a copy
/ of this software and associated documentation files (the "Software"), to deal
/ in the Software without restriction, including without limitation the rights to
/ use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
/ of the Software, and to permit persons to whom the Software is furnished to
/ do so, subject to the following conditions:
/ 
/ The above copyright notice and this permission notice shall be included in all
/ copies or substantial portions of the Software.
/ 
/ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
/ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
/ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
/ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
/ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
/ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
/ OTHER DEALINGS IN THE SOFTWARE.
/
******************************************************************************/

#include "stdafx.h"

// GetSetObjectState can take a very long time to read and/or write, especially with some FX.
// To mitigate these issues when dealing with operations that require many reads/writes,
// Call SWS_GetSetObjectState and SWS_FreeHeapPtr instead.  When you want to cache your
// state reads and writes call SWS_CacheObjectState(true).  When done call SWS_CacheObjectState(false)
// and any changes will be written out.
//
// See Snapshots for an example of use

//#define GOS_DEBUG

ObjectStateCache::ObjectStateCache()
{
}

ObjectStateCache::~ObjectStateCache()
{
	EmptyCache();
}

void ObjectStateCache::WriteCache()
{
	for (int i = 0; i < m_obj.GetSize(); i++)
	{
		if (m_str.Get(i)->GetLength() && m_orig.Get(i) && strcmp(m_str.Get(i)->Get(), m_orig.Get(i)))
			GetSetObjectState(m_obj.Get(i), m_str.Get(i)->Get());
	}

	EmptyCache();
}

void ObjectStateCache::EmptyCache()
{
	m_obj.Empty(false);
	m_str.Empty(true);
	m_orig.Empty(true, FreeHeapPtr);
}

char* ObjectStateCache::GetSetObjState(void* obj, const char* str)
{
	int i = m_obj.Find(obj);
	if (i < 0)
	{
		i = m_obj.GetSize();
		m_obj.Add(obj);
		m_str.Add(new WDL_String);
		if (str && str[0])
			m_orig.Add(NULL);
		else
			m_orig.Add(GetSetObjectState(obj, NULL));
	}
	if (str && str[0])
	{
		m_str.Get(i)->Set(str);
		return NULL;
	}

	if (m_str.Get(i)->GetLength())
		return m_str.Get(i)->Get();
	else
		return m_orig.Get(i);
}

ObjectStateCache* g_objStateCache = NULL;


char* SWS_GetSetObjectState(void* obj, const char* str)
{
	char* ret;

	if (g_objStateCache)
		ret = g_objStateCache->GetSetObjState(obj, str);
	else
		ret = GetSetObjectState(obj, str);

#ifdef GOS_DEBUG
	char debugStr[4096];
	_snprintf(debugStr, 4096, "GetSetObjectState call, obj %08X, IN:\n%s\n\nOUT:\n%s\n\n", obj, str ? str : "NULL", ret ? ret : "NULL");
	OutputDebugString(debugStr);
#endif

	return ret;
}

void SWS_FreeHeapPtr(void* ptr)
{
	// Ignore frees on cached object states, 
	if (!g_objStateCache)
		FreeHeapPtr(ptr);
}

void SWS_CacheObjectState(bool bStart)
{
	if (bStart)
	{
		delete g_objStateCache;
		g_objStateCache = new ObjectStateCache;
	}
	else if (g_objStateCache)
	{
		g_objStateCache->WriteCache();
		delete g_objStateCache;
		g_objStateCache = NULL;
	}
}

// Helper function for parsing object "chunks" into more useful lines
// newlines are retained.  Caller allocates the WDL_String necessary for the output
// pos stores the state of the line parsing, set to zero to return the first line
// Return of true means valid line, false is "end of chunk"
bool GetChunkLine(const char* chunk, WDL_String* line, int* pos, bool bNewLine)
{
	int chunkLen = (int)strlen(chunk);
	if (*pos >= chunkLen || *pos < 0)
		return false;

	const char* pEnd = strchr(chunk + *pos, '\n');
	int iEndPos;
	if (!pEnd)
		iEndPos = chunkLen;
	else
		iEndPos = 1 + pEnd - chunk;

	if (line)
		line->Set(chunk + *pos, iEndPos - *pos + (bNewLine ? 0 : -1));
	*pos = iEndPos;
	return true;
}

void AppendChunkLine(WDL_String* chunk, const char* line)
{
	// Insert a line into the chunk before the closing >
	char* pIns = strrchr(chunk->Get(), '>');
	if (!pIns)
		return;
	int pos = pIns - chunk->Get();
	if (line[strlen(line)-1] != '\n')
		chunk->Insert("\n", pos);
	chunk->Insert(line, pos);
}

bool GetChunkFromProjectState(const char* cSection, WDL_String* chunk, const char* line, ProjectStateContext *ctx)
{
	if (strncmp(line, cSection, strlen(cSection)) == 0)
	{
		int iDepth = 1;
		char linebuf[4096];
		chunk->Set(line);
		chunk->Append("\n");
		while (iDepth)
		{
			ctx->GetLine(linebuf, 4096);
			chunk->Append(linebuf);
			chunk->Append("\n");
			if (linebuf[0] == '<')
				iDepth++;
			else if (linebuf[0] == '>')
				iDepth--;
		}
		return true;
	}
	return false;
}