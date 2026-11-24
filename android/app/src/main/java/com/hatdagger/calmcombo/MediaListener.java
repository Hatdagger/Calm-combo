package com.hatdagger.calmcombo;

import android.service.notification.NotificationListenerService;

/**
 * Does nothing by itself.  Android only shows an app the song playing in
 * other apps (their media sessions) if the user has given it notification
 * access, and that access is granted to a listener like this one.  Calm Combo
 * never reads notifications; it only asks for the media sessions.
 */
public class MediaListener extends NotificationListenerService {
}
