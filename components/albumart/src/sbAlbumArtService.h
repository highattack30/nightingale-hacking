/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 :miv */
/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2008 POTI, Inc.
// http://songbirdnest.com
//
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the "GPL").
//
// Software distributed under the License is distributed
// on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either
// express or implied. See the GPL for the specific language
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// END SONGBIRD GPL
//
*/

#ifndef __SB_ALBUMARTSERVICE_H__
#define __SB_ALBUMARTSERVICE_H__

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//
// Songbird album art service.
//
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

/**
 * \file  sbAlbumArtService.h
 * \brief Songbird Album Art Service Definitions.
 */

//------------------------------------------------------------------------------
//
// Songbird album art service imported services.
//
//------------------------------------------------------------------------------

// Songbird imports.
#include <sbIAlbumArtFetcher.h>
#include <sbIAlbumArtService.h>
#include <sbIMediaList.h>
#include <sbIMediaListListener.h>
#include <sbMemoryUtils.h>
#include <sbLibraryUtils.h>

// Mozilla imports.
#include <nsCOMPtr.h>
#include <nsIFileStreams.h>
#include <nsIIOService.h>
#include <nsIMIMEService.h>
#include <nsIObserver.h>
#include <nsIObserverService.h>
#include <nsStringAPI.h>
#include <nsStringGlue.h>
#include <nsTArray.h>
#include <nsInterfaceHashtable.h>
#include <nsITimer.h>
#include <set>

//------------------------------------------------------------------------------
//
// Songbird album art service defs.
//
//------------------------------------------------------------------------------

//
// Songbird album art service component defs.
//

#define SB_ALBUMARTSERVICE_CLASSNAME "sbAlbumArtService"
#define SB_ALBUMARTSERVICE_CID                                                 \
  /* {8DF4920B-89AC-49A3-8552-D74D313C62B6} */                                 \
  {                                                                            \
    0x8DF4920B,                                                                \
    0x89AC,                                                                    \
    0x49A3,                                                                    \
    { 0x85, 0x52, 0xD7, 0x4D, 0x31, 0x3C, 0x62, 0xB6 }                         \
  }

typedef std::set<nsString>    sbStringSet;
typedef sbStringSet::iterator sbStringSetIter;

//------------------------------------------------------------------------------
//
// Songbird album art service classes.
//
//------------------------------------------------------------------------------

/**
 * This class implements the album art service component.
 */

class sbAlbumArtService : public sbIAlbumArtService,
                          public sbIMediaListListener,
                          public nsIObserver
{
  //----------------------------------------------------------------------------
  //
  // Public interface.
  //
  //----------------------------------------------------------------------------

public:

  //
  // Inherited interfaces.
  //

  NS_DECL_ISUPPORTS
  NS_DECL_SBIALBUMARTSERVICE
  NS_DECL_SBIMEDIALISTLISTENER
  NS_DECL_NSIOBSERVER


  //
  // Public services.
  //

  sbAlbumArtService();

  virtual ~sbAlbumArtService();

  nsresult Initialize();


  //----------------------------------------------------------------------------
  //
  // Private interface.
  //
  //----------------------------------------------------------------------------

private:

  //
  // Fetcher information.
  //
  //   contractID                   Album art fetcher contract ID.
  //   fetcher                      Album art fetcher.
  //   priority                     Priority of the fetcher (used for sorting).
  //   enabled                      used to filter our disabled fetchers.
  //

  class FetcherInfo
  {
  public:
    nsCString                       contractID;
    PRUint32                        priority;
    PRBool                          enabled;
    PRBool                          local;

    // Defined for the Sort function on nsTArray
    PRBool operator<(const FetcherInfo& right) const {
      return (priority < right.priority);
    };
    
    // This has to be defined for Sort as well, but is used for Searching.
    PRBool operator==(const FetcherInfo& right) const {
      return contractID.Equals(right.contractID);
    }
  };

  //
  // mObserverService           Observer service.
  // mIOService                 I/O service.
  // mMIMEService               MIME service.
  // mAlbumArtCacheDir          Album art cache directory.
  // mInitialized               True if album art service initialized.
  // mPrefsAvailable            True if preferences are available.
  // mFetcherInfoList           List of fetcher information.
  // mValidExtensionList        List of valid album art file extensions.
  // mTemporaryCache            Hash of arbitrary data used by art fetchers
  // mCacheFlushTimer           Timer used to empty the temporary cache
  //

  nsCOMPtr<nsIObserverService>  mObserverService;
  nsCOMPtr<nsIIOService>        mIOService;
  nsCOMPtr<nsIMIMEService>      mMIMEService;
  nsCOMPtr<nsIFile>             mAlbumArtCacheDir;
  PRBool                        mInitialized;
  PRBool                        mPrefsAvailable;
  nsTArray<FetcherInfo>         mFetcherInfoList;
  nsTArray<nsCString>           mValidExtensionList;
  nsInterfaceHashtable<nsStringHashKey, nsISupports> 
                                mTemporaryCache;
  nsCOMPtr<nsITimer>            mCacheFlushTimer;
  
  //
  // mMainLibraryList           Main library we monitor for changes.
  // mBatchCounter              Keep track of the batches running
  // mAlbumArtUrlSet            Array of URLs to check for "cleaning"
  // mAlbumArtCleanTimer        Timer for checking if album art files need to
  //                            be removed.
  //
  
  nsCOMPtr<sbIMediaList>        mMainLibraryList;
  sbLibraryBatchHelper          mBatchHelper;
  sbStringSet                   mAlbumArtUrlSet;
  nsCOMPtr<nsITimer>            mAlbumArtCleanTimer;
  
  //
  // Internal services.
  //

  //
  // InitializeLibraryWatch
  // 

  nsresult InitializeLibraryWatch();

  void Finalize();

  nsresult GetAlbumArtCacheDir();

  nsresult GetAlbumArtFetcherInfo();

  nsresult UpdateAlbumArtFetcherInfo();

  nsresult GetCacheFileBaseName(const PRUint8* aData,
                                PRUint32       aDataLen,
                                nsAString&     aFileBaseName);

  nsresult GetAlbumArtFileExtension(const nsACString& aMimeType,
                                    nsAString&        aFileExtension);

  nsresult StoreAlbumArtUrlForCheck(nsString& aCacheURL);
  
  nsresult SaveSetToFile();
  nsresult LoadSetFromFile();

  nsresult ScanAlbumArtUrls();

  nsresult CheckAlbumArtCache(const nsAString& aCacheURL);

  nsresult RemoveAlbumArtCache(const nsAString& aCacheURL);

};


//
// Auto-disposal class wrappers.
//
//   sbAutoFileOutputStream     Wrapper to auto close an nsIFileOutputStream.
//

SB_AUTO_CLASS(sbAutoFileOutputStream,
              nsCOMPtr<nsIFileOutputStream>,
              mValue,
              mValue->Close(),
              mValue = nsnull);

#endif // __SB_ALBUMARTSERVICE_H__

