/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2007 POTI, Inc.
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

#include "sbLocalDatabasePropertyCache.h"
#include <prlog.h>
#include <DatabaseQuery.h>
#include <sbSQLBuilderCID.h>
#include <nsMemory.h>
#include <nsCOMArray.h>
#include <nsStringEnumerator.h>

#define INIT if (!mInitialized) { rv = Init(); NS_ENSURE_SUCCESS(rv, rv); }

#define MAX_IN_LENGTH 100

#if defined PR_LOGGING
static const PRLogModuleInfo *gLocalDatabasePropertyCacheLog = nsnull;
#define LOG(args) PR_LOG(gLocalDatabasePropertyCacheLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

NS_IMPL_ISUPPORTS1(sbLocalDatabasePropertyCache, sbILocalDatabasePropertyCache)

struct sbStaticProperty {
  const char* mName;
  const char* mColumn;
  PRUint32    mID;
};

static sbStaticProperty kStaticProperties[] = {
  {
    "http://songbirdnest.com/data/1.0#created",
    "created",
    PR_UINT32_MAX,
  },
  {
    "http://songbirdnest.com/data/1.0#updated",
    "updated",
    PR_UINT32_MAX - 1,
  },
  {
    "http://songbirdnest.com/data/1.0#contentUrl",
    "content_url",
    PR_UINT32_MAX - 2,
  },
  {
    "http://songbirdnest.com/data/1.0#contentMimeType",
    "content_mime_type",
    PR_UINT32_MAX - 3,
  },
  {
    "http://songbirdnest.com/data/1.0#contentLength",
    "content_length",
    PR_UINT32_MAX - 4,
  }
};

sbLocalDatabasePropertyCache::sbLocalDatabasePropertyCache() :
  mInitialized(PR_FALSE)
{
  mNumStaticProperties = sizeof(kStaticProperties) / sizeof(kStaticProperties[0]);

  #ifdef PR_LOGGING
  if (!gLocalDatabasePropertyCacheLog) {
    gLocalDatabasePropertyCacheLog = PR_NewLogModule("LocalDatabasePropertyCache");
  }
#endif
}

sbLocalDatabasePropertyCache::~sbLocalDatabasePropertyCache()
{
}

NS_IMETHODIMP
sbLocalDatabasePropertyCache::GetDatabaseGUID(nsAString& aDatabaseGUID)
{
  aDatabaseGUID = mDatabaseGUID;

  return NS_OK;
}
NS_IMETHODIMP
sbLocalDatabasePropertyCache::SetDatabaseGUID(const nsAString& aDatabaseGUID)
{
  mDatabaseGUID = aDatabaseGUID;

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabasePropertyCache::CacheProperties(const PRUnichar **aGUIDArray,
                                              PRUint32 aGUIDArrayCount)
{

  nsresult rv;

  INIT;

  /*
   * First, collect all the guids that are not cached
   */
  nsTArray<nsString> misses;
  for (PRUint32 i = 0; i < aGUIDArrayCount; i++) {
    nsAutoString guid(aGUIDArray[i]);
    if (!mCache.Get(guid, nsnull)) {
      NS_ENSURE_TRUE(misses.AppendElement(guid),
                     NS_ERROR_OUT_OF_MEMORY);
    }
  }

  /*
   * Look up and cache each of the misses
   */
  PRUint32 numMisses = misses.Length();
  if (numMisses > 0) {
    PRUint32 inNum = 0;
    for (PRUint32 j = 0; j < numMisses; j++) {

      /*
       * Add each guid to the query and execute the query when we've added
       * MAX_IN_LENGTH of them (or when we are on the last one)
       */
      rv = mPropertiesInCriterion->AddString(misses[j]);
      NS_ENSURE_SUCCESS(rv, rv);

      if (inNum > MAX_IN_LENGTH || j + 1 == numMisses) {
        PRInt32 dbOk;

        nsAutoString sql;
        rv = mPropertiesSelect->ToString(sql);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<sbIDatabaseQuery> query;
        rv = MakeQuery(sql, getter_AddRefs(query));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = query->Execute(&dbOk);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_SUCCESS(dbOk, dbOk);

        rv = query->WaitForCompletion(&dbOk);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_SUCCESS(dbOk, dbOk);

        nsCOMPtr<sbIDatabaseResult> result;
        rv = query->GetResultObject(getter_AddRefs(result));
        NS_ENSURE_SUCCESS(rv, rv);

        PRInt32 rowCount;
        rv = result->GetRowCount(&rowCount);
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString lastGUID;
        nsCOMPtr<sbILocalDatabaseResourcePropertyBag> bag;
        for (PRInt32 row = 0; row < rowCount; row++) {
          PRUnichar* guid;
          rv = result->GetRowCellPtr(row, 0, &guid);
          NS_ENSURE_SUCCESS(rv, rv);

          /*
           * If this is the first row result or we've encountered a new
           * guid, create a new property bag and add it to the cache
           */
          if (row == 0 || !lastGUID.Equals(guid)) {
            lastGUID = guid;
            bag = new sbLocalDatabaseResourcePropertyBag(this);
            rv = NS_STATIC_CAST(sbLocalDatabaseResourcePropertyBag*, bag.get())
                                  ->Init();
            NS_ENSURE_SUCCESS(rv, rv);
            NS_ENSURE_TRUE(mCache.Put(lastGUID, bag),
                           NS_ERROR_OUT_OF_MEMORY);
          }

          /*
           * Add each property / object pair to the current bag
           */
          nsAutoString propertyIDStr;
          rv = result->GetRowCell(row, 1, propertyIDStr);
          NS_ENSURE_SUCCESS(rv, rv);

          PRUint32 propertyID = propertyIDStr.ToInteger(&rv);
          NS_ENSURE_SUCCESS(rv, rv);

          nsAutoString obj;
          rv = result->GetRowCell(row, 2, obj);
          NS_ENSURE_SUCCESS(rv, rv);

          rv = NS_STATIC_CAST(sbLocalDatabaseResourcePropertyBag*, bag.get())
                                ->PutValue(propertyID, obj);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        mPropertiesInCriterion->Clear();
        NS_ENSURE_SUCCESS(rv, rv);
      }

    }

    /*
     * Do the same thing for top level properties
     */
    inNum = 0;
    for (PRUint32 j = 0; j < numMisses; j++) {

      rv = mMediaItemsInCriterion->AddString(misses[j]);
      NS_ENSURE_SUCCESS(rv, rv);

      if (inNum > MAX_IN_LENGTH || j + 1 == numMisses) {
        PRInt32 dbOk;

        nsAutoString sql;
        rv = mMediaItemsSelect->ToString(sql);
        NS_ENSURE_SUCCESS(rv, rv);

        nsCOMPtr<sbIDatabaseQuery> query;
        rv = MakeQuery(sql, getter_AddRefs(query));
        NS_ENSURE_SUCCESS(rv, rv);

        rv = query->Execute(&dbOk);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_SUCCESS(dbOk, dbOk);

        rv = query->WaitForCompletion(&dbOk);
        NS_ENSURE_SUCCESS(rv, rv);
        NS_ENSURE_SUCCESS(dbOk, dbOk);

        nsCOMPtr<sbIDatabaseResult> result;
        rv = query->GetResultObject(getter_AddRefs(result));
        NS_ENSURE_SUCCESS(rv, rv);

        PRInt32 rowCount;
        rv = result->GetRowCount(&rowCount);
        NS_ENSURE_SUCCESS(rv, rv);

        sbILocalDatabaseResourcePropertyBag* bag;
        for (PRInt32 row = 0; row < rowCount; row++) {
          nsAutoString guid;
          rv = result->GetRowCell(row, 0, guid);
          NS_ENSURE_SUCCESS(rv, rv);

          if (!mCache.Get(guid, &bag)) {
            bag = new sbLocalDatabaseResourcePropertyBag(this);
            rv = NS_STATIC_CAST(sbLocalDatabaseResourcePropertyBag*, bag)
                                  ->Init();
            NS_ENSURE_SUCCESS(rv, rv);
            NS_ENSURE_TRUE(mCache.Put(guid, bag),
                           NS_ERROR_OUT_OF_MEMORY);
          }

          for (PRUint32 i = 0; i < mNumStaticProperties; i++) {
            nsAutoString value;
            rv = result->GetRowCell(row, i + 1, value);
            NS_ENSURE_SUCCESS(rv, rv);

            rv = NS_STATIC_CAST(sbLocalDatabaseResourcePropertyBag*, bag)
                                  ->PutValue(kStaticProperties[i].mID, value);
            NS_ENSURE_SUCCESS(rv, rv);
          }

        }

        mMediaItemsInCriterion->Clear();
        NS_ENSURE_SUCCESS(rv, rv);
      }

    }

  }

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabasePropertyCache::GetProperties(const PRUnichar **aGUIDArray,
                                            PRUint32 aGUIDArrayCount,
                                            PRUint32 *aPropertyArrayCount,
                                            sbILocalDatabaseResourcePropertyBag ***aPropertyArray)
{
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aPropertyArrayCount);
  NS_ENSURE_ARG_POINTER(aPropertyArray);

  rv = CacheProperties(aGUIDArray, aGUIDArrayCount);
  NS_ENSURE_SUCCESS(rv, rv);

  /*
   * Build the output array using cache lookups
   */
  sbILocalDatabaseResourcePropertyBag **propertyBagArray = nsnull;

  *aPropertyArrayCount = aGUIDArrayCount;
  if (aGUIDArrayCount > 0) {

    propertyBagArray = (sbILocalDatabaseResourcePropertyBag **)
      nsMemory::Alloc((sizeof (sbILocalDatabaseResourcePropertyBag *)) * *aPropertyArrayCount);

    for (PRUint32 i = 0; i < aGUIDArrayCount; i++) {
      nsAutoString guid(aGUIDArray[i]);
      sbILocalDatabaseResourcePropertyBag* bag;
      if (mCache.Get(guid, &bag)) {
        propertyBagArray[i] = bag;
        NS_ADDREF(propertyBagArray[i]);
      }
      else {
        propertyBagArray[i] = nsnull;
      }
    }
  }
  else {
    *propertyBagArray = nsnull;
  }

  *aPropertyArray = propertyBagArray;

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabasePropertyCache::Init()
{
  nsresult rv;

  /*
   * Simple select from properties table with an in list of guids
   */
  mPropertiesSelect = do_CreateInstance(SB_SQLBUILDER_SELECT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPropertiesSelect->SetBaseTableName(NS_LITERAL_STRING("resource_properties"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPropertiesSelect->AddColumn(EmptyString(), NS_LITERAL_STRING("guid"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPropertiesSelect->AddColumn(EmptyString(), NS_LITERAL_STRING("property_id"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPropertiesSelect->AddColumn(EmptyString(), NS_LITERAL_STRING("obj"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPropertiesSelect->CreateMatchCriterionIn(EmptyString(),
                                                 NS_LITERAL_STRING("guid"),
                                                 getter_AddRefs(mPropertiesInCriterion));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPropertiesSelect->AddCriterion(mPropertiesInCriterion);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mPropertiesSelect->AddOrder(EmptyString(),
                                   NS_LITERAL_STRING("guid"),
                                   PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  /*
   * Create simple media_items query with in list of guids
   */
  rv = mPropertiesSelect->AddOrder(EmptyString(),
                                   NS_LITERAL_STRING("property_id"),
                                   PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  mMediaItemsSelect = do_CreateInstance(SB_SQLBUILDER_SELECT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMediaItemsSelect->SetBaseTableName(NS_LITERAL_STRING("media_items"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMediaItemsSelect->AddColumn(EmptyString(), NS_LITERAL_STRING("guid"));
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRUint32 i = 0; i < mNumStaticProperties; i++) {
    rv = mMediaItemsSelect->AddColumn(EmptyString(),
                                      NS_ConvertUTF8toUTF16(kStaticProperties[i].mColumn));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = mMediaItemsSelect->CreateMatchCriterionIn(EmptyString(),
                                                 NS_LITERAL_STRING("guid"),
                                                 getter_AddRefs(mMediaItemsInCriterion));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMediaItemsSelect->AddCriterion(mMediaItemsInCriterion);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mMediaItemsSelect->AddOrder(EmptyString(),
                                   NS_LITERAL_STRING("guid"),
                                   PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = LoadProperties();
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(mCache.Init(1000), NS_ERROR_OUT_OF_MEMORY);

  mInitialized = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabasePropertyCache::MakeQuery(const nsAString& aSql,
                                        sbIDatabaseQuery** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  LOG(("MakeQuery: %s", NS_ConvertUTF16toUTF8(aSql).get()));

  nsresult rv;

  nsCOMPtr<sbIDatabaseQuery> query =
    do_CreateInstance(SONGBIRD_DATABASEQUERY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->SetDatabaseGUID(mDatabaseGUID);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->SetAsyncQuery(PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->AddQuery(aSql);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = query);
  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabasePropertyCache::LoadProperties()
{
  nsresult rv;
  PRInt32 dbOk;

  if (!mPropertyNameToID.IsInitialized()) {
    NS_ENSURE_TRUE(mPropertyNameToID.Init(100), NS_ERROR_OUT_OF_MEMORY);
  }
  else {
    mPropertyNameToID.Clear();
  }

  if (!mPropertyIDToName.IsInitialized()) {
    NS_ENSURE_TRUE(mPropertyIDToName.Init(100), NS_ERROR_OUT_OF_MEMORY);
  }
  else {
    mPropertyIDToName.Clear();
  }

  nsCOMPtr<sbISQLSelectBuilder> builder =
    do_CreateInstance(SB_SQLBUILDER_SELECT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = builder->SetBaseTableName(NS_LITERAL_STRING("properties"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = builder->AddColumn(EmptyString(), NS_LITERAL_STRING("property_id"));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = builder->AddColumn(EmptyString(), NS_LITERAL_STRING("property_name"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString sql;
  rv = builder->ToString(sql);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<sbIDatabaseQuery> query;
  rv = MakeQuery(sql, getter_AddRefs(query));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = query->Execute(&dbOk);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(dbOk, dbOk);

  rv = query->WaitForCompletion(&dbOk);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_SUCCESS(dbOk, dbOk);

  nsCOMPtr<sbIDatabaseResult> result;
  rv = query->GetResultObject(getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 rowCount;
  rv = result->GetRowCount(&rowCount);
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < rowCount; i++) {
    nsAutoString propertyIDStr;
    rv = result->GetRowCell(i, 0, propertyIDStr);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 propertyID = propertyIDStr.ToInteger(&rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoString propertyName;
    rv = result->GetRowCell(i, 1, propertyName);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<nsString> propertyNamePtr(new nsString(propertyName));
    NS_ENSURE_TRUE(mPropertyIDToName.Put(propertyID, propertyNamePtr),
                   NS_ERROR_OUT_OF_MEMORY);
    propertyNamePtr.forget();

    NS_ENSURE_TRUE(mPropertyNameToID.Put(propertyName, propertyID),
                   NS_ERROR_OUT_OF_MEMORY);

  }

  /*
   * Add top level properties
   */
  for (PRUint32 i = 0; i < mNumStaticProperties; i++) {

    /*
     * Convert the char* constants to nsString
     */
    nsCAutoString propertyNameCString(kStaticProperties[i].mName);
    nsAutoString propertyName(NS_ConvertUTF8toUTF16(propertyNameCString).get());

    nsAutoPtr<nsString> propertyNamePtr(new nsString(propertyName));
    NS_ENSURE_TRUE(mPropertyIDToName.Put(kStaticProperties[i].mID, propertyNamePtr),
                   NS_ERROR_OUT_OF_MEMORY);
    propertyNamePtr.forget();
    NS_ENSURE_TRUE(mPropertyNameToID.Put(propertyName, kStaticProperties[i].mID),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}

PRUint32
sbLocalDatabasePropertyCache::GetPropertyID(const nsAString& aPropertyName)
{
  PRUint32 retval;
  if (!mPropertyNameToID.Get(aPropertyName, &retval)) {
    retval = 0;
  }
  return retval;
}

PRBool
sbLocalDatabasePropertyCache::GetPropertyName(PRUint32 aPropertyID,
                                              nsAString& aPropertyName)
{
  nsString *propertyName;
  if (mPropertyIDToName.Get(aPropertyID, &propertyName)) {
    aPropertyName = *propertyName;
    return PR_TRUE;
  }
  return PR_FALSE;
}

// sbILocalDatabaseResourcePropertyBag
NS_IMPL_ISUPPORTS1(sbLocalDatabaseResourcePropertyBag,
                   sbILocalDatabaseResourcePropertyBag)

sbLocalDatabaseResourcePropertyBag::sbLocalDatabaseResourcePropertyBag(sbLocalDatabasePropertyCache* aCache) :
  mCache(aCache)
{
}

sbLocalDatabaseResourcePropertyBag::~sbLocalDatabaseResourcePropertyBag()
{
}

NS_IMETHODIMP
sbLocalDatabaseResourcePropertyBag::Init()
{
  NS_ENSURE_TRUE(mValueMap.Init(), NS_ERROR_OUT_OF_MEMORY);
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
PropertyBagKeysToArray(const PRUint32& aPropertyID,
                       nsString* aValue,
                       void *aArg)
{
  nsTArray<PRUint32>* propertyIDs = (nsTArray<PRUint32>*) aArg;
  if (propertyIDs->AppendElement(aPropertyID)) {
    return PL_DHASH_NEXT;
  }
  else {
    return PL_DHASH_STOP;
  }
}

NS_IMETHODIMP
sbLocalDatabaseResourcePropertyBag::GetNames(nsIStringEnumerator **aNames)
{
  NS_ENSURE_ARG_POINTER(aNames);

  nsTArray<PRUint32> propertyIDs;
  mValueMap.EnumerateRead(PropertyBagKeysToArray, &propertyIDs);

  PRUint32 len = mValueMap.Count();
  if (propertyIDs.Length() < len) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsTArray<nsString> propertyNames;
/*
  XXX: these commented out bits are waiting for NS_NewAdoptingStringEnumerator
       to be included in the glue
  nsStringArray *array = new nsStringArray(len);
  NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);
*/
  for (PRUint32 i = 0; i < len; i++) {
    nsAutoString propertyName;
    NS_ENSURE_TRUE(mCache->GetPropertyName(propertyIDs[i], propertyName),
                   NS_ERROR_UNEXPECTED);
/*
    NS_ENSURE_TRUE(array->InsertStringAt(propertyName, i),
                   NS_ERROR_OUT_OF_MEMORY);
*/
    propertyNames.AppendElement(propertyName);
  }

/*
  NS_NewAdoptingStringEnumerator(aNames, array);
*/
  *aNames = new sbTArrayStringEnumerator(&propertyNames);
  NS_ENSURE_TRUE(*aNames, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*aNames);

  return NS_OK;
}

NS_IMETHODIMP
sbLocalDatabaseResourcePropertyBag::GetProperty(const nsAString& aName,
                                                nsAString& _retval)
{
  PRUint32 propertyID = mCache->GetPropertyID(aName);
  if(propertyID > 0) {
    nsString* value;
    if (mValueMap.Get(propertyID, &value)) {
      _retval = *value;
      return NS_OK;
    }
  }

  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
sbLocalDatabaseResourcePropertyBag::PutValue(PRUint32 aPropertyID,
                                             const nsAString& aValue)
{
  nsAutoPtr<nsString> value(new nsString(aValue));
  NS_ENSURE_TRUE(mValueMap.Put(aPropertyID, value),
                 NS_ERROR_OUT_OF_MEMORY);
  value.forget();
  return NS_OK;
}

NS_IMPL_ISUPPORTS1(sbTArrayStringEnumerator,
                   nsIStringEnumerator)

sbTArrayStringEnumerator::sbTArrayStringEnumerator(nsTArray<nsString>* aStringArray) :
  mNextIndex(0)
{
  mStringArray.InsertElementsAt(0, *aStringArray);
}

sbTArrayStringEnumerator::~sbTArrayStringEnumerator()
{
}

NS_IMETHODIMP
sbTArrayStringEnumerator::HasMore(PRBool *_retval)
{
  *_retval = mNextIndex < mStringArray.Length();
  return NS_OK;
}

NS_IMETHODIMP
sbTArrayStringEnumerator::GetNext(nsAString& _retval)
{
  if (mNextIndex < mStringArray.Length()) {
    _retval = mStringArray[mNextIndex];
    mNextIndex++;
    return NS_OK;
  }
  else {
    return NS_ERROR_NOT_AVAILABLE;
  }
}

