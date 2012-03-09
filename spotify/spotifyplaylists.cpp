/**  This file is part of QT SpotifyWebApi - <hugolm84@gmail.com> ===
 *
 *   Copyright 2011-2012,Hugo Lindström <hugolm84@gmail.com>
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 */

#include "spotifyplaylists.h"
#include "callbacks.h"
#include <QObject>
#include <QThread>
SpotifyPlaylists::SpotifyPlaylists( QObject *parent )
   : QObject( parent )
{

    /**
        Read the QSettings to set the sync states from previous settings
    **/

    QSettings settings;
    int size = settings.beginReadArray( "syncPlaylists" );

    for ( int i = 0; i < size; ++i )
    {
         settings.setArrayIndex( i );
         Sync sync;
         sync.id_ = settings.value( "id" ).toString();
         qDebug() << sync.id_;
         sync.sync_ = settings.value( "sync" ).toBool();
         m_syncPlaylists.append( sync );
         setSyncPlaylist( sync.id_ );
    }

    settings.endArray();
}


/**
  Destructor
    This destructor is important.
    It removes callbacks and frees playlists and its tracks
**/

SpotifyPlaylists::~SpotifyPlaylists()
{

    qDebug() << "Destroying playlists";
    for ( int i = 0 ; i < sp_playlistcontainer_num_playlists( SpotifySession::getInstance()->PlaylistContainer() ) ; ++i )
    {
        sp_playlist* playlist = sp_playlistcontainer_playlist( SpotifySession::getInstance()->PlaylistContainer(), i );
 //       qDebug() << "Remvoing callbacks on " << sp_playlist_name( playlist );
        sp_playlist_remove_callbacks( playlist, &SpotifyCallbacks::playlistCallbacks, SpotifySession::getInstance()->Playlists() );
        if( i < m_playlists.size() )
        {
            sp_playlist_release( m_playlists[i].playlist_ );
            foreach( sp_track *track, m_playlists[i].tracks_ )
            {
    //            qDebug() << "Releasing track" << sp_track_name( track );
                sp_track_release( track );
            }
        }

    }

}

/**
  Callback
    State changed
    Called from libspotify when state changed on playlist
**/
void
SpotifyPlaylists::stateChanged( sp_playlist* pl, void* userdata )
{

    qDebug() << Q_FUNC_INFO;

    SpotifyPlaylists* _playlists = reinterpret_cast<SpotifyPlaylists*>( userdata );
    qDebug() << "Callback on thread" << _playlists->thread()->currentThreadId();

    // If the playlist isn't loaded yet we have to wait
    if ( !sp_playlist_is_loaded( pl ) )
    {
      qDebug() << "Playlist isn't loaded yet, waiting";
      return;
    }
    _playlists->addPlaylist( pl );
}


/**
  Callback
    State changed
    Called from libspotify when state changed on playlist
    @note: Will send the playlist when loaded, at startup
**/

void
SpotifyPlaylists::syncStateChanged( sp_playlist* pl, void* userdata )
{
    qDebug() << "Sync state changed";
    if ( !sp_playlist_is_loaded( pl ) )
    {
      qDebug() << "Playlist isn't loaded yet, waiting";
      return;
    }
    SpotifyPlaylists* _playlists = reinterpret_cast<SpotifyPlaylists*>( userdata );
    _playlists->doSend( _playlists->getLoadedPlaylist( pl ) ); //_playlists->doSend();
}

/**
 getLoadedPLaylist( sp_playlist )
   Gets a specific playlist from the list with id (uri)
**/
SpotifyPlaylists::LoadedPlaylist
SpotifyPlaylists::getLoadedPlaylist( sp_playlist *&playlist )
{
    LoadedPlaylist pl;
    pl.playlist_ = playlist;
    int index = m_playlists.indexOf( pl );
    if( index != -1)
        return m_playlists.at( index );
    return pl;
}

/**
  Callback
    State changed
    Called from libspotify when state changed on playlist
**/

void
SpotifyPlaylists::playlistContainerLoadedCallback( sp_playlistcontainer* pc, void* userdata)
{

    SpotifySession* _session = reinterpret_cast<SpotifySession*>( userdata );

    for ( int i = 0 ; i < sp_playlistcontainer_num_playlists( pc ) ; ++i )
    {
        sp_playlist* playlist = sp_playlistcontainer_playlist( pc, i );
        sp_playlist_add_callbacks( playlist, &SpotifyCallbacks::playlistCallbacks, _session->Playlists() );
    }
    /**
      This creates the starred tracks playlist, and will automatically add it to the synclist
    **/
    sp_playlist* starredTracks = sp_session_starred_create( _session->Session() );
    sp_playlist_add_callbacks( starredTracks, &SpotifyCallbacks::syncPlaylistCallbacks, _session->Playlists() );

    QString name = sp_user_canonical_name( sp_session_user( _session->Session() ) );
    QString starredId = "spotify:user:" + name + ":playlist:0000000000000000000000";
    _session->Playlists()->addPlaylist( starredTracks );
    _session->Playlists()->setSyncPlaylist( starredId );
    _session->setPlaylistContainer( pc );

    qDebug() << Q_FUNC_INFO << "done";

}

/**
  Callback
    PlaylistAdded
    Called from libspotify when a new playlist is added
**/

void
SpotifyPlaylists::playlistAddedCallback( sp_playlistcontainer* pc, sp_playlist* playlist, int position, void* userdata ) {

    Q_UNUSED( playlist );
    Q_UNUSED( userdata );
    qDebug() << "Playlist added";

}
/**
  Callback
    State changed
    Called from libspotify when state changed on playlist
**/

void
SpotifyPlaylists::playlistMovedCallback( sp_playlistcontainer* pc, sp_playlist* playlist, int position, int new_position, void* userdata ) {

    qDebug() << "Playlist Moved";
    SpotifySession* _session = reinterpret_cast<SpotifySession*>( userdata );
     _session->Playlists()->setPosition( playlist, position, new_position );
}
/**
  Callback
    State changed
    Called from libspotify when state changed on playlist
**/

void
SpotifyPlaylists::playlistRemovedCallback( sp_playlistcontainer* pc, sp_playlist* playlist, int position, void* userdata ) {

    Q_UNUSED( position );

    qDebug() << "Playlist removed";

    SpotifySession* _session = reinterpret_cast<SpotifySession*>( userdata );
    _session->Playlists()->removePlaylist( playlist );
    _session->setPlaylistContainer( pc );
}

/**
  Callback
    State changed
    Called from libspotify when state changed on playlist
**/

void
SpotifyPlaylists::tracksAdded(sp_playlist *pl, sp_track * const *tracks, int num_tracks, int position, void *userdata)
{
    qDebug() << "Tracks Added";
    SpotifyPlaylists* _playlists = reinterpret_cast<SpotifyPlaylists*>( userdata );
    _playlists->addTracks( pl, tracks, num_tracks, position);
}



/**
   addTracks(sp_playlist*, sp_tracks * const *tracks, int num_tracks)
**/
void
SpotifyPlaylists::addTracks(sp_playlist* pl, sp_track *const*tracks, int num_tracks, int pos)
{
    qDebug() << "Adding tracks to" << sp_playlist_name(pl);

    LoadedPlaylist playlist;
    playlist.playlist_ = pl;

    const int index = m_playlists.indexOf( playlist );
    if( index != -1 ){

        for ( int i=0 ; i< num_tracks; ++i )
        {
            qDebug() << "Pos" << pos;
            sp_track* track = *(tracks++);
            qDebug() << "Adding track " << i << sp_track_name( track );
            sp_track_add_ref( track );
            m_playlists[index].tracks_.insert(pos, track );
            pos++;
        }

    }

}

/**
  removeTracks(sp_playlist*, const int*tracks, int num_tracks)
**/
void
SpotifyPlaylists::removeTracks(sp_playlist* pl, const int *tracks, int num_tracks)
{
    qDebug() << "Removing tracks from " << sp_playlist_name(pl);

    LoadedPlaylist playlist;
    playlist.playlist_ = pl;

    const int index = m_playlists.indexOf( playlist );
    if( index != -1 ){

        if( num_tracks == m_playlists[index].tracks_.count() )
            m_playlists[index].tracks_.clear();

        for ( int i=0 ; i< num_tracks; ++i )
        {
            int pos = *(tracks)++;
            qDebug() << "Remvoing track at" << pos;
            m_playlists[index].tracks_.removeAt( pos );
        }
    }

}
/**
 getPLaylist( const QString )
   Gets a specific playlist from the list with id (uri)
**/
SpotifyPlaylists::LoadedPlaylist
SpotifyPlaylists::getPlaylist( const QString id )
{
    LoadedPlaylist pl;
    pl.id_ = id;
    int index = m_playlists.indexOf( pl );
    if( index != -1)
        return m_playlists.at( index );
    return pl;
}

/**
 setSyncPlaylist( const QString )
   sets syncflags on a playlist
   Saves state to QSettings
**/
void
SpotifyPlaylists::setSyncPlaylist( const QString id )
{
    LoadedPlaylist pl;
    pl.id_ = id;
    qDebug() << "Setting sync for " << id;
    int index = m_playlists.indexOf( pl );
    if( index != -1 )
    {
         // Set QSettings to be able to remember state on startup

        Sync syncThis;
        syncThis.id_ = id;
        syncThis.sync_ = true;
        if( !m_syncPlaylists.contains( syncThis ) )
        {
             m_syncPlaylists.append( syncThis );
             QSettings settings;
             settings.beginWriteArray("syncPlaylists");
             for ( int i = 0; i < m_syncPlaylists.size(); ++i )
             {
                 settings.setArrayIndex( i );
                 settings.setValue( "id" , m_syncPlaylists.at( i ).id_ );
                 settings.setValue( "sync" , m_syncPlaylists.at( i ).sync_ );
             }
             settings.endArray();
         }
        m_playlists[ index ].sync_ = true;
        sp_playlist_add_callbacks( m_playlists[ index ].playlist_, &SpotifyCallbacks::syncPlaylistCallbacks, this);
    }
}


/**
  tracksMoved
  **/
void SpotifyPlaylists::tracksMoved(sp_playlist *playlist, const int *tracks, int num_tracks, int new_position, void *userdata)
{

    SpotifyPlaylists* _playlists = reinterpret_cast<SpotifyPlaylists*>( userdata );
    LoadedPlaylist pl;
    pl.playlist_ = playlist;

    qDebug() << "Numtracks:" << num_tracks << " tracks:" << *tracks;

    int index = _playlists->getPlaylists().indexOf( pl );
    if( index != -1)
    {
        for(int i = 0; i < num_tracks; i++)
        {
            qDebug() << "Moving track nr" << i << "at pos " << tracks[i] << " to pos" << new_position;
            _playlists->getPlaylists()[index].tracks_.move(tracks[i], new_position++);

        }

        for(int i = 0; i < _playlists->getPlaylists()[index].tracks_.count(); i++)
            qDebug() << "Track" << sp_track_name(_playlists->getPlaylists()[index].tracks_[i]);
    }

    qDebug() << "Tracks moved";

}

/**
 setPosition( sp_playlist *, int, int )
   Updates the position of the playlist, if moved
**/
void
SpotifyPlaylists::setPosition( sp_playlist *playlist, int oPos, int nPost )
{
    LoadedPlaylist pl;
    pl.playlist_ = playlist;

    int index = m_playlists.indexOf( pl );

    if( index != -1)
        if( m_playlists.count() > nPost )
            m_playlists.move( oPos, nPost ); //( index );

}

/**
 setPosition( sp_playlist *, int, int )
   Updates the position of the playlist, if moved
**/
void
SpotifyPlaylists::removePlaylist( sp_playlist *playlist )
{
    LoadedPlaylist pl;
    pl.playlist_ = playlist;

    int index = m_playlists.indexOf( pl );

    if( index != -1)
        m_playlists.removeAt( index );

}

/**
 setPlaylistInProgress( sp_playlist *, bool)
   This function is called from callback
   Sets a bool on the playlist if its loading or not
**/
void
SpotifyPlaylists::setPlaylistInProgress( sp_playlist *pl, bool done )
{
    qDebug() << "In Progress" << sp_playlist_name(pl);

    LoadedPlaylist playlist;
    playlist.playlist_ = pl;

    const int index = m_playlists.indexOf( playlist );

    if( index != -1 ){
        qDebug() << "Playlist progress is" << (done ? "done" : "still loading..." ) << index;
        m_playlists[ index ].isLoaded = done;
        if(done && m_playlists[index].sync_)
            doSend( m_playlists[index] ); //_playlists->doSend();
    }

}



/**
 addPlaylist( sp_playlist *)
   This function is called from callback
   This will also fire if and of the playlist callbacks is called,
   thus updateing the list-eg. if any track is moved, it will rearange the order.
**/
void
SpotifyPlaylists::addPlaylist( sp_playlist *pl )
{


    LoadedPlaylist playlist;
    playlist.playlist_ = pl;

    int index = m_playlists.indexOf( playlist );
    if( index != -1 )
        return;

    qDebug() << "Adding" << sp_playlist_name(pl);

    playlist.starContainer_ = false;
    playlist.sync_ = false;
    playlist.isLoaded = false;
    char linkStr[256];
    sp_link *pl_link = sp_link_create_from_playlist( pl );
    sp_link_as_string( pl_link, linkStr, sizeof(linkStr));
    sp_link_release( pl_link );
    playlist.id_ = linkStr;
    playlist.name_ = sp_playlist_name(pl);

    /**
      Starred playlist folder will get a 0 hash as linkstr
      Use that to set the id
    **/
    if( playlist.id_.contains( "0000000000000000000000" ) )
    {
        playlist.name_ =  "Starred Tracks";
        playlist.starContainer_ = true;
    }

    for ( int i=0 ; i< sp_playlist_num_tracks( pl ); ++i )
    {
        sp_track* track = sp_playlist_track( pl, i );
        sp_track_add_ref( track );
        playlist.tracks_.push_back( track );
    }

    // Playlist is loaded and ready
    playlist.isLoaded = true;

    // If the list contains the playlist, update it
    // else, append it

    if(!m_playlists.contains( playlist ) )
    {
        m_playlists.append( playlist );

        /**
          Initially, when reading QSettings for syncPlaylists
          we cant set the callbacks directly, as they are not loaded yet.
          This will add sync callbacks for the loaded playlists,
        **/
        Sync syncThis;
        syncThis.id_ = playlist.id_;
        if( m_syncPlaylists.contains( syncThis ) )
        {
            qDebug() << "Adding syncing for  playlist " << playlist.id_;
            setSyncPlaylist( playlist.id_ );
        }
    }
    /*else
    {
        qDebug() << "Something changed, will replace at index" << m_playlists.indexOf( playlist );
        m_playlists.replace( m_playlists.indexOf( playlist ), playlist);
    }*/


}

/**
  operator==
**/
bool operator==(SpotifyPlaylists::LoadedPlaylist one, SpotifyPlaylists::LoadedPlaylist two)
{
    if(one.id_ == two.id_)
        return true;
    if( one.playlist_ == two.playlist_)
        return true;
    return false;
}
bool operator==(SpotifyPlaylists::Sync one, SpotifyPlaylists::Sync two)
{
    if(one.id_ == two.id_)
        return true;
    return false;
}