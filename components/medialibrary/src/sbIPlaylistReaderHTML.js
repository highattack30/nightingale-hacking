/*
 //
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyright� 2006 Pioneers of the Inevitable LLC
// http://songbirdnest.com
// 
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the �GPL�).
// 
// Software distributed under the License is distributed 
// on an �AS IS� basis, WITHOUT WARRANTY OF ANY KIND, either 
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

//
// sbIPlaylistReader Object (HTML)
//

const SONGBIRD_PLAYLISTHTML_CONTRACTID = "@songbird.org/Songbird/Playlist/Reader/HTML;1";
const SONGBIRD_PLAYLISTHTML_CLASSNAME = "Songbird HTML Playlist Interface";
const SONGBIRD_PLAYLISTHTML_CID = Components.ID("{63607801-7CA1-45fe-BD31-8659E43173D4}");
const SONGBIRD_PLAYLISTHTML_IID = Components.interfaces.sbIPlaylistReader;

function CPlaylistHTML()
{
  jsLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"].getService(Components.interfaces.mozIJSSubScriptLoader);
  jsLoader.loadSubScript( "chrome://Songbird/content/scripts/songbird_interfaces.js", this );
}

/* I actually need a constructor in this case. */
CPlaylistHTML.prototype.constructor = CPlaylistHTML;

/* the CPlaylistHTML class def */
CPlaylistHTML.prototype = 
{
  originalURL: "",

  m_HREFArray: new Array(),

  Read: function( strURL, strGUID, strDestTable, replace, /* out */ errorCode )
  {
    try
    {
      // Twick the filename off the end of the url.
      if ( this.originalURL && this.originalURL.length )
      {
        this.originalURL = this.originalURL.substr( 0, this.originalURL.lastIndexOf("/") ) + "/";
      }
      
      // Setup the file input stream and pass it to a function to parse the html
      var retval = false;
      errorCode.value = 0;
      
      var pFileReader = (Components.classes["@mozilla.org/network/file-input-stream;1"]).createInstance(Components.interfaces.nsIFileInputStream);
      var pFile = (Components.classes["@mozilla.org/file/local;1"]).createInstance(Components.interfaces.nsILocalFile);
      var pURI = (Components.classes["@mozilla.org/network/simple-uri;1"]).createInstance(Components.interfaces.nsIURI);

      if ( pFile && pURI && pFileReader )
      {
        pURI.spec = strURL;
        if ( pURI.scheme == "file" )
        {
          var cstrPathToPLS = pURI.path;
          cstrPathToPLS = cstrPathToPLS.substr( 3, cstrPathToPLS.length ); // .Cut(0, 3); //remove '///'
          pFile.initWithPath(cstrPathToPLS);
          if ( pFile.isFile() )
          {
            pFileReader.init( pFile, /* PR_RDONLY */ 1, 0, /*nsIFileInputStream::CLOSE_ON_EOF*/ 0 );
            retval = this.HandleHTMLStream( strGUID, strDestTable, pFileReader, replace );
            pFileReader.close();
          }
        }
      }
      return retval;
    }
    catch ( err )
    {
      throw "CPlaylistHTML::Read - " + err;
    }
  },
  
  Vote: function( strURL )
  {
    try
    {
      return 10000;
    }
    catch ( err )
    {
      throw "CPlaylistHTML::Vote - " + err;
    }
  },
  
  Name: function()
  {
    try
    {
      return "HTML Playlist"; // ????
    }
    catch ( err )
    {
      throw "CPlaylistHTML::Name - " + err;
    }
  },
  
  Description: function()
  {
    try
    {
      return "HTML Playlist"; // ????
    }
    catch ( err )
    {
      throw "CPlaylistHTML::Description - " + err;
    }
  },
  
  SupportedMIMETypes: function( /* out */ nMIMECount )
  {
    var retval = new Array;
    nMIMECount.value = 0;

    try
    {
      retval.push( "text/html" );
      nMIMECount.value = retval.length;
    }
    catch ( err )
    {
      throw "CPlaylistHTML::SupportedMIMETypes - " + err;
    }
    return retval;
  },
  
  SupportedFileExtensions: function( /* out */ nExtCount )
  {
    var retval = new Array;
    nExtCount.value = 0;
    
    try
    {
      retval.push( "html" );
      retval.push( "htm" );
      retval.push( "php" );
      retval.push( "php3" );
      retval.push( "" );
      
      nExtCount.value = retval.length;
    }
    catch ( err )
    {
      throw "CPlaylistHTML::SupportedFileExtensions - " + err;
    }
    
    return retval;
  },
  
  HandleHTMLStream: function( strGUID, strDestTable, streamIn, replace )
  {
    var retval = false;
    try
    {
      streamIn = streamIn.QueryInterface( Components.interfaces.nsILineInputStream );
      var links = new Array();
      var inTag = false;
      var inATag = false;
      // Parse the file (line by line! woo hoo!)
      var out = {};
      var count = 0;
      while ( streamIn.readLine( out ) )
      {
        ++count;
        var line = out.value;
        while ( line.length && line.length > 0 )
        {
          // What state were we in when we last got in here?
          if ( inTag )
          {
            // See if we can find an href attribute
            if ( inATag )
            {
              var href_idx = line.indexOf( 'href="' );
              if ( href_idx > -1 )
              {
                var temp = line.substr( href_idx + 6, line.length );
                var href = temp.substr( 0, temp.indexOf('"') );
                if ( this.IsMediaUrl( href ) )
                {
                  if ( href.indexOf( '://' ) == -1 )
                  {
                    href = this.originalURL + href;
                  }
                  links.push( href );
                }
              }
            }
            // Look for the closing of a tag
            var close = line.indexOf( '>' );
            if ( close > -1 )
            {
              inTag = false;
              inATag = false;
              line = line.substr( close + 1, line.length );
            }
            else
            {
              break; // EOL
            }
          }
          else
          {
            // Look for the opening of a tag
            var open = line.indexOf( '<' );
            if ( open > -1 )
            {
              inTag = true;
              line = line.substr( open + 1, line.length );
              if ( ( line[ 0 ] == "a" ) || ( line[ 0 ] == "A" ) )
              {
                inATag = true;
              }
            }
            else
            {
              break; // EOL
            }
          }
        }
      }
      // I'm tired of this function.  Let's have a different one.
      retval = this.CreatePlaylist( strGUID, strDestTable, links, replace );
    }
    catch ( err )
    {
      throw "CPlaylistHTML::HandleHTMLBuffer - " + err;
    }
    return retval;
  },
  
  CreatePlaylist: function( strGUID, strDestTable, links_array, replace )
  {
    try
    {
      retval = false;
      if ( links_array.length > 0 )
      {
        var pQuery = new this.sbIDatabaseQuery();
        pQuery.SetAsyncQuery(true);
        pQuery.SetDatabaseGUID(strGUID);

        const MediaLibrary = new Components.Constructor("@songbird.org/Songbird/MediaLibrary;1", "sbIMediaLibrary");
        var pLibrary = new MediaLibrary();
        const PlaylistManager = new Components.Constructor("@songbird.org/Songbird/PlaylistManager;1", "sbIPlaylistManager");
        var pPlaylistManager = new PlaylistManager();
        
        pLibrary.SetQueryObject(pQuery);
        pPlaylist = pPlaylistManager.GetPlaylist(strDestTable, pQuery);

        var inserted = new Array();        
        for ( var i in links_array )
        {
          var url = links_array[ i ];
          
          // Don't insert it if we already did.
          var skip = false;
          for ( var j in inserted )
          {
            if ( inserted[ j ] == url )
            {
              skip = true;
              break;
            }
          }
        
          if ( !skip )
          {
            var aMetaKeys = new Array("title");
            var aMetaValues = new Array( this.ConvertUrlToDisplayName( url ) );

            var guid = pLibrary.AddMedia( url, aMetaKeys.length, aMetaKeys, aMetaValues.length, aMetaValues, replace, true );
            pPlaylist.AddByGUID( guid, strGUID, -1, replace, true );
            
            inserted.push( url );
          }
        }
        pQuery.Execute();
        retval = true;
      }
      return retval;
    }
    catch ( err )
    {
      throw "CPlaylistHTML::HandleHTMLBuffer - " + err;
    }
  },

  IsMediaUrl: function( the_url )
  {
    if ( ( the_url.indexOf ) && 
          (
            // Protocols at the beginning
            ( the_url.indexOf( "mms:" ) == 0 ) || 
            ( the_url.indexOf( "rtsp:" ) == 0 ) || 
            // File extensions at the end
            ( the_url.indexOf( ".pls" ) != -1 ) || 
            ( the_url.indexOf( "rss" ) != -1 ) || 
            ( the_url.indexOf( ".m3u" ) == ( the_url.length - 4 ) ) || 
//            ( the_url.indexOf( ".rm" ) == ( the_url.length - 3 ) ) || 
//            ( the_url.indexOf( ".ram" ) == ( the_url.length - 4 ) ) || 
//            ( the_url.indexOf( ".smil" ) == ( the_url.length - 5 ) ) || 
            ( the_url.indexOf( ".mp3" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".ogg" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".flac" ) == ( the_url.length - 5 ) ) ||
            ( the_url.indexOf( ".wav" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".m4a" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".wma" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".wmv" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".asx" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".asf" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".avi" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".mov" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".mpg" ) == ( the_url.length - 4 ) ) ||
            ( the_url.indexOf( ".mp4" ) == ( the_url.length - 4 ) )
          )
        )
    {
      return true;
    }
    return false;
  },
  
  ConvertUrlToDisplayName: function( url )
  {
    // Set the title display  
    var the_value = "";
    if ( url.lastIndexOf('/') != -1 )
    {
      the_value = url.substring( url.lastIndexOf('/') + 1, url.length );
    }
    else if ( url.lastIndexOf('\\') != -1 )
    {
      the_value = url.substring( url.lastIndexOf('\\') + 1, url.length );
    }
    else
    {
      the_value = url;
    }
    // Convert any %XX to space
    var percent = the_value.indexOf('%');
    if ( percent != -1 )
    {
      var remainder = the_value;
      the_value = "";
      while ( percent != -1 )
      {
        the_value += remainder.substring( 0, percent );
        remainder = remainder.substring( percent + 3, url.length );
        percent = remainder.indexOf('%');
        the_value += " ";
        if ( percent == -1 )
        {
          the_value += remainder;
        }
      }
    }
    if ( the_value.length == 0 )
    {
      the_value = url;
    }
    return the_value;
  },

  QueryInterface: function(iid)
  {
      if (!iid.equals(Components.interfaces.nsISupports) &&
          !iid.equals(SONGBIRD_PLAYLISTHTML_IID))
          throw Components.results.NS_ERROR_NO_INTERFACE;
      return this;
  }
};

/**
 * \class sbPlaylistHTMLModule
 * \brief 
 */
var sbPlaylistHTMLModule = 
{
  //firstTime: true,
  
  registerSelf: function(compMgr, fileSpec, location, type)
  {
    //if(this.firstTime)
    //{
      //this.firstTime = false;
      //throw Components.results.NS_ERROR_FACTORY_REGISTER_AGAIN;
    //}
 
    compMgr = compMgr.QueryInterface(Components.interfaces.nsIComponentRegistrar);
    compMgr.registerFactoryLocation(SONGBIRD_PLAYLISTHTML_CID, 
                                    SONGBIRD_PLAYLISTHTML_CLASSNAME, 
                                    SONGBIRD_PLAYLISTHTML_CONTRACTID, 
                                    fileSpec, 
                                    location,
                                    type);
  },

  getClassObject: function(compMgr, cid, iid) 
  {
    if(!cid.equals(SONGBIRD_PLAYLISTHTML_CID))
        throw Components.results.NS_ERROR_NO_INTERFACE;

    if(!iid.equals(Components.interfaces.nsIFactory))
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

    return sbPlaylistHTMLFactory;
  },

  canUnload: function(compMgr)
  { 
    return true; 
  }
};

/**
 * \class sbPlaylistHTMLFactory
 * \brief 
 */
var sbPlaylistHTMLFactory =
{
  createInstance: function(outer, iid)
  {
    if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;

    if (!iid.equals(SONGBIRD_PLAYLISTHTML_IID) &&
        !iid.equals(Components.interfaces.nsISupports))
        throw Components.results.NS_ERROR_INVALID_ARG;

    return (new CPlaylistHTML()).QueryInterface(iid);
  }
}; //sbPlaylistHTMLFactory

/**
 * \function NSGetModule
 * \brief 
 */
function NSGetModule(comMgr, fileSpec)
{ 
  return sbPlaylistHTMLModule;
} //NSGetModule
