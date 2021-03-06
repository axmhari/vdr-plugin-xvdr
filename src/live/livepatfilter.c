/*
 *      vdr-plugin-xvdr - XBMC server plugin for VDR
 *
 *      Copyright (C) 2010 Alwin Esch (Team XBMC)
 *      Copyright (C) 2010, 2011 Alexander Pipelka
 *
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "config/config.h"
#include "tools/hash.h"

#include "livepatfilter.h"
#include "livereceiver.h"
#include "livestreamer.h"

static const char * const psStreamTypes[] = {
        "UNKNOWN",
        "ISO/IEC 11172 Video",
        "ISO/IEC 13818-2 Video",
        "ISO/IEC 11172 Audio",
        "ISO/IEC 13818-3 Audio",
        "ISO/IEC 13818-1 Privete sections",
        "ISO/IEC 13818-1 Private PES data",
        "ISO/IEC 13512 MHEG",
        "ISO/IEC 13818-1 Annex A DSM CC",
        "0x09",
        "ISO/IEC 13818-6 Multiprotocol encapsulation",
        "ISO/IEC 13818-6 DSM-CC U-N Messages",
        "ISO/IEC 13818-6 Stream Descriptors",
        "ISO/IEC 13818-6 Sections (any type, including private data)",
        "ISO/IEC 13818-1 auxiliary",
        "ISO/IEC 13818-7 Audio with ADTS transport sytax",
        "ISO/IEC 14496-2 Visual (MPEG-4)",
        "ISO/IEC 14496-3 Audio with LATM transport syntax",
        "0x12", "0x13", "0x14", "0x15", "0x16", "0x17", "0x18", "0x19", "0x1a",
        "ISO/IEC 14496-10 Video (MPEG-4 part 10/AVC, aka H.264)",
        "",
};

cLivePatFilter::cLivePatFilter(cLiveStreamer *Streamer, const cChannel *Channel)
{
  DEBUGLOG("cStreamdevPatFilter(\"%s\")", Channel->Name());
  m_Channel     = Channel;
  m_Streamer    = Streamer;
  m_pmtPid      = 0;
  m_pmtSid      = 0;
  m_pmtVersion  = -1;
  Set(0x00, 0x00);  // PAT

}

void cLivePatFilter::GetLanguage(SI::PMT::Stream& stream, char *langs, int& type)
{
  SI::Descriptor *d;
  for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); )
  {
    switch (d->getDescriptorTag())
    {
      case SI::ISO639LanguageDescriptorTag:
      {
        SI::ISO639LanguageDescriptor *ld = (SI::ISO639LanguageDescriptor *)d;
        strn0cpy(langs, I18nNormalizeLanguageCode(ld->languageCode), MAXLANGCODE1);
        SI::Loop::Iterator it;
        SI::ISO639LanguageDescriptor::Language first;
        type = 0;
        if (ld->languageLoop.getNext(first, it)) {
          type = first.getAudioType();
        }
        break;
      }
      default: ;
    }
    delete d;
  }
}

bool cLivePatFilter::GetStreamInfo(SI::PMT::Stream& stream, struct StreamInfo& info)
{
  SI::Descriptor *d;

  info.pid = stream.getPid();

  if (!info.pid)
    return false;

  switch (stream.getStreamType())
  {
    case 0x01: // ISO/IEC 11172 Video
    case 0x02: // ISO/IEC 13818-2 Video
    case 0x80: // ATSC Video MPEG2 (ATSC DigiCipher QAM)
      DEBUGLOG("PMT scanner adding PID %d (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()]);
      info.type = stMPEG2VIDEO;
      return true;

    case 0x03: // ISO/IEC 11172 Audio
    case 0x04: // ISO/IEC 13818-3 Audio
      info.type = stMPEG2AUDIO;
      GetLanguage(stream, info.lang, info.audioType);
      DEBUGLOG("PMT scanner adding PID %d (%s) (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()], info.lang);
      return true;

    case 0x0f: // ISO/IEC 13818-7 Audio with ADTS transport syntax
      info.type = stAAC;
      GetLanguage(stream, info.lang, info.audioType);
      DEBUGLOG("PMT scanner: adding PID %d (%s) %s (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "AAC", info.lang);
      return true;

    case 0x11: // ISO/IEC 14496-3 Audio with LATM transport syntax
      info.type = stLATM;
      GetLanguage(stream, info.lang, info.audioType);
      DEBUGLOG("PMT scanner: adding PID %d (%s) %s (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "LATM", info.lang);
      return true;

#if 1
    case 0x07: // ISO/IEC 13512 MHEG
    case 0x08: // ISO/IEC 13818-1 Annex A  DSM CC
    case 0x0a: // ISO/IEC 13818-6 Multiprotocol encapsulation
    case 0x0b: // ISO/IEC 13818-6 DSM-CC U-N Messages
    case 0x0c: // ISO/IEC 13818-6 Stream Descriptors
    case 0x0d: // ISO/IEC 13818-6 Sections (any type, including private data)
    case 0x0e: // ISO/IEC 13818-1 auxiliary
#endif
    case 0x10: // ISO/IEC 14496-2 Visual (MPEG-4)
      DEBUGLOG("PMT scanner: Not adding PID %d (%s) (skipped)\n", stream.getPid(), psStreamTypes[stream.getStreamType()]);
      break;

    case 0x1b: // ISO/IEC 14496-10 Video (MPEG-4 part 10/AVC, aka H.264)
      DEBUGLOG("PMT scanner adding PID %d (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()]);
      info.type = stH264;
      return true;

    case 0x05: // ISO/IEC 13818-1 private sections
    case 0x06: // ISO/IEC 13818-1 PES packets containing private data
      for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); )
      {
        switch (d->getDescriptorTag())
        {
          case SI::AC3DescriptorTag:
            info.type = stAC3;
            GetLanguage(stream, info.lang, info.audioType);
            DEBUGLOG("PMT scanner: adding PID %d (%s) %s (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "AC3", info.lang);
            delete d;
            return true;

          case SI::EnhancedAC3DescriptorTag:
            info.type = stEAC3;
            GetLanguage(stream, info.lang, info.audioType);
            DEBUGLOG("PMT scanner: adding PID %d (%s) %s (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "EAC3", info.lang);
            delete d;
            return true;

          case SI::DTSDescriptorTag:
            info.type = stDTS;
            GetLanguage(stream, info.lang, info.audioType);
            DEBUGLOG("PMT scanner: adding PID %d (%s) %s (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "DTS", info.lang);
            delete d;
            return true;

          case SI::AACDescriptorTag:
            info.type = stAAC;
            GetLanguage(stream, info.lang, info.audioType);
            DEBUGLOG("PMT scanner: adding PID %d (%s) %s (%s)\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "AAC", info.lang);
            delete d;
            return true;

          case SI::TeletextDescriptorTag:
            info.type = stTELETEXT;
            DEBUGLOG("PMT scanner: adding PID %d (%s) %s\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "Teletext");
            delete d;
            return true;

          case SI::SubtitlingDescriptorTag:
          {
            info.type              = stDVBSUB;
            info.subtitlingType    = 0;
            info.compositionPageId = 0;
            info.ancillaryPageId   = 0;
            SI::SubtitlingDescriptor *sd = (SI::SubtitlingDescriptor *)d;
            SI::SubtitlingDescriptor::Subtitling sub;
            char *s = info.lang;
            int n = 0;
            for (SI::Loop::Iterator it; sd->subtitlingLoop.getNext(sub, it); )
            {
              if (sub.languageCode[0])
              {
                info.subtitlingType    = sub.getSubtitlingType();
                info.compositionPageId = sub.getCompositionPageId();
                info.ancillaryPageId   = sub.getAncillaryPageId();
                if (n > 0)
                  *s++ = '+';
                strn0cpy(s, I18nNormalizeLanguageCode(sub.languageCode), MAXLANGCODE1);
                s += strlen(s);
                if (n++ > 1)
                  break;
              }
            }
            delete d;
            DEBUGLOG("PMT scanner: adding PID %d (%s) %s\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "DVBSUB");
            return true;

          }
          default:
            DEBUGLOG("PMT scanner: NOT adding PID %d (%s) %s (%i)\n", stream.getPid(), psStreamTypes[stream.getStreamType()], "UNKNOWN", d->getDescriptorTag());
            break;
        }
        delete d;
      }
      break;

    default:
      /* This following section handles all the cases where the audio track
       * info is stored in PMT user info with stream id >= 0x81
       * we check the registration format identifier to see if it
       * holds "AC-3"
       */
      if (stream.getStreamType() >= 0x81)
      {
        bool found = false;
        for (SI::Loop::Iterator it; (d = stream.streamDescriptors.getNext(it)); )
        {
          switch (d->getDescriptorTag())
          {
            case SI::RegistrationDescriptorTag:
            /* unfortunately libsi does not implement RegistrationDescriptor */
            if (d->getLength() >= 4)
            {
              found = true;
              SI::CharArray rawdata = d->getData();
              if (/*rawdata[0] == 5 && rawdata[1] >= 4 && */
                  rawdata[2] == 'A' && rawdata[3] == 'C' &&
                  rawdata[4] == '-' && rawdata[5] == '3')
              {
                DEBUGLOG("PMT scanner: Adding pid %d (type 0x%x) RegDesc len %d (%c%c%c%c)\n",
                            stream.getPid(), stream.getStreamType(), d->getLength(), rawdata[2], rawdata[3], rawdata[4], rawdata[5]);
                info.type = stAC3;
                delete d;
                return true;
              }
            }
            break;
            default:
            break;
          }
          delete d;
        }
        if (!found)
        {
          DEBUGLOG("NOT adding PID %d (type 0x%x) RegDesc not found -> UNKNOWN\n", stream.getPid(), stream.getStreamType());
        }
      }
      DEBUGLOG("PMT scanner: NOT adding PID %d (%s) %s\n", stream.getPid(), psStreamTypes[stream.getStreamType()<0x1c?stream.getStreamType():0], "UNKNOWN");
      break;
  }

  info.type = stNONE;
  return false;
}

void cLivePatFilter::Process(u_short Pid, u_char Tid, const u_char *Data, int Length)
{
  if (Pid == 0x00 && Tid == 0x00)
  {
    SI::PAT pat(Data, false);
    if (!pat.CheckCRCAndParse())
      return;

    SI::PAT::Association assoc;
    for (SI::Loop::Iterator it; pat.associationLoop.getNext(assoc, it); )
    {
      if (!assoc.isNITPid())
      {
        const cChannel *Channel =  Channels.GetByServiceID(Source(), Transponder(), assoc.getServiceId());
        if (Channel && (Channel == m_Channel))
        {
          int prevPmtPid = m_pmtPid;
          if (0 != (m_pmtPid = assoc.getPid()))
          {
            m_pmtSid = assoc.getServiceId();
            if (m_pmtPid != prevPmtPid)
            {
              Add(m_pmtPid, 0x02);
              m_pmtVersion = -1;
            }
            return;
          }
        }
      }
    }
  }
  else if (Pid == m_pmtPid && Tid == SI::TableIdPMT && Source() && Transponder())
  {
    SI::PMT pmt(Data, false);
    if (!pmt.CheckCRCAndParse() || pmt.getServiceId() != m_pmtSid)
      return;

    if (m_pmtVersion != -1)
    {
      if (m_pmtVersion != pmt.getVersionNumber())
      {
        cFilter::Del(m_pmtPid, 0x02);
        m_pmtPid = 0; // this triggers PAT scan
      }
      return;
    }
    m_pmtVersion = pmt.getVersionNumber();

    // get cached channel data
    if(m_ChannelCache.size() == 0)
      m_ChannelCache = cChannelCache::GetFromCache(CreateChannelUID(m_Channel));

    // get all streams and check if there are new (currently unknown) streams
    SI::PMT::Stream stream;
    cChannelCache cache;
    for (SI::Loop::Iterator it; pmt.streamLoop.getNext(stream, it); )
    {
      struct StreamInfo info;
      if (GetStreamInfo(stream, info) && cache.size() < MAXRECEIVEPIDS)
        cache.AddStream(info);
    }

    // no new streams found -> exit
    if (cache == m_ChannelCache)
      return;

    m_Streamer->m_FilterMutex.Lock();

    // create new stream demuxers
    cache.CreateDemuxers(m_Streamer);

    INFOLOG("Currently unknown new streams found, requesting stream change");

    // write changed data back to the cache
    m_ChannelCache = cache;
    cChannelCache::AddToCache(CreateChannelUID(m_Channel), m_ChannelCache);

    m_Streamer->RequestStreamChange();
    m_Streamer->m_FilterMutex.Unlock();
  }
}
