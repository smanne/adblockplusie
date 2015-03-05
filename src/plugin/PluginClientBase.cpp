/*
 * This file is part of Adblock Plus <https://adblockplus.org/>,
 * Copyright (C) 2006-2015 Eyeo GmbH
 *
 * Adblock Plus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * Adblock Plus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Adblock Plus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PluginClientBase.h"

#include <memory>
#include <WinInet.h>
#include <shlwapi.h>

#include "PluginSettings.h"
#include "Config.h"
#include "PluginDebug.h"

CComAutoCriticalSection LogQueue::s_criticalSectionQueue;
std::vector<CPluginError> LogQueue::s_pluginErrors;


void UnescapeUrl(std::wstring& url)
{
  try
  {
    DWORD result_length = INTERNET_MAX_URL_LENGTH;
    std::unique_ptr<wchar_t[]> result(new wchar_t[result_length]);
    HRESULT hr = UrlUnescapeW(const_cast<wchar_t*>(url.c_str()), result.get(), &result_length, 0);
    if (hr == S_OK)
    {
      url = std::wstring(result.get(), result_length);
    }
    /*
     * Do nothing. This masks error return values from UrlUnescape without logging the error.
     */
  }
  catch(std::bad_alloc e)
  {
    /*
     * When the code has a systematic way of handling bad_alloc, we'll rethrow (probably).
     * Until then, we mask the exception and make no modification.
     */
  }
  catch(...)
  {
    // no modification if any other exception
  }
}

void LogQueue::LogPluginError(DWORD errorCode, int errorId, int errorSubid, const std::string& description, bool isAsync, DWORD dwProcessId, DWORD dwThreadId)
{
  // Prevent circular references
  if (CPluginSettings::HasInstance() && isAsync)
  {
    DEBUG_ERROR_CODE_EX(errorCode, description, dwProcessId, dwThreadId);

    CString pluginError;
    pluginError.Format(L"%2.2d%2.2d", errorId, errorSubid);

    CString pluginErrorCode;
    pluginErrorCode.Format(L"%u", errorCode);

    CPluginSettings* settings = CPluginSettings::GetInstance();

    settings->AddError(pluginError, pluginErrorCode);
  }

  // Post error to client for later submittal
  if (!isAsync)
  {
    LogQueue::PostPluginError(errorId, errorSubid, errorCode, description);
  }
}


void LogQueue::PostPluginError(int errorId, int errorSubid, DWORD errorCode, const std::string& errorDescription)
{
  s_criticalSectionQueue.Lock();
  {
    CPluginError pluginError(errorId, errorSubid, errorCode, errorDescription);

    s_pluginErrors.push_back(pluginError);
  }
  s_criticalSectionQueue.Unlock();
}


bool LogQueue::PopFirstPluginError(CPluginError& pluginError)
{
  bool hasError = false;

  s_criticalSectionQueue.Lock();
  {
    std::vector<CPluginError>::iterator it = s_pluginErrors.begin();
    if (it != s_pluginErrors.end())
    {
      pluginError = *it;

      hasError = true;

      s_pluginErrors.erase(it);
    }
  }
  s_criticalSectionQueue.Unlock();

  return hasError;
}
