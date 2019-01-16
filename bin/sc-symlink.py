#!/usr/bin/env python3

import argparse
import glob
import gzip
import os
import re
import sys
import xml.etree.ElementTree as ET

video_extension = ['.mp4', '.webm']
youtube_regex = re.compile(r'youtu.be|youtube.com')
video_id_regexes = [
    re.compile(r'https://www.youtube.com/embed/([A-Za-z0-9_-]{11})'),
    re.compile(r'https://player.twitch.tv/\?video=(v[0-9]+)'),
]

races_short = {
    0: '',
    1: 'P',
    2: 'T',
    3: 'Z'
}

def out(prefix, str):
    sys.stdout.write(prefix)
    sys.stdout.write(str)
    sys.stdout.write('\n')

def error(str):
    return out('ERROR: ', str)

def warn(str):
    return out('WARN: ', str)

def info(str):
    out('INFO: ', str)

try:
    parser = argparse.ArgumentParser(description='create symlinks for Starcraft VODs')
    parser.add_argument('--vod-dir', help='directory containg the VOD files', required=True)
    parser.add_argument('--symlink-dir', help='directory to create the symlinks in', required=True)
    parser.add_argument('--xml-dir', help='directory containing harbour-starfish XMLs', required=True, action='append')
    parser.add_argument('--relative', help='make symlinks relative if possible', default=False, action='store_true')
    args = parser.parse_args()

    symlink_dir = os.path.abspath(args.symlink_dir)
    vod_dir = os.path.abspath(args.vod_dir)

    # if args.relative:
    #     ancestor = os.path.commonprefix([symlink_dir, vod_dir])
    #     if ancestor != '/':
    #         vod_dir = os.path.relpath(vod_dir, symlink_dir)

    if vod_dir[-1] != '/':
        vod_dir += '/'

    if symlink_dir[-1] != '/':
        symlink_dir += '/'


    def get_video_id(str):
        for r in video_id_regexes:
            m = r.search(str)
            if m:
                return m.group(1)

    def make_name(record):
        def matchup():
            if 'race1' in record.attrib and 'race2' in record.attrib:
                r1 = int(record.attrib['race1'])
                r2 = int(record.attrib['race2'])
                return '{}v{}'.format(races_short[r1], races_short[r2])

        def sides():
            if 'side1' in record.attrib and 'side2' in record.attrib:
                return record.attrib['side1'] + ' vs ' + record.attrib['side2']


        mu = matchup()
        if mu:
            name = mu + ' '
        else:
            name = ''

        s = sides()
        if s:
            name += s
        else:
            name += record.attrib['match_name']

        return name

    def get_season(record):
        if 'season' in record.attrib:
            return record.attrib['season']
        return 1

    def get_game(record):
        game = int(record.attrib['game'])
        if game == 0:
            return "Broodwar"
        if game == 1:
            return "Starcraft II"
        if game == 2:
            return "Overwatch"
        return 'unknown'


    def get_vod_file_path(id):
        for file_path in glob.glob('{}{}.*'.format(vod_dir, id)):
            for ext in video_extension:
                if file_path.endswith(ext):
                    return file_path



    def process_xml(file_name, xml):
        try:
            et = ET.fromstring(xml)
            # <record year="2017" event_full_name="WardiTV" event_name="WardiTV" url="https://www.youtube.com/embed/nRfDuYEVQ9g?start=0&amp;showinfo=0" side1="Scarlett" match_name="Scarlett vs Awers" side2="Awers" game="1" race1="1" race2="2" version="1" stage="Weekly" match_date="2017-12-30"/>
            for record in et.findall('.//record'):
                url = record.attrib['url']
                id = get_video_id(url)
                if not id:
                    warn('Failed to get video id for url {}'.format(url))
                    continue

                vod_file_path = get_vod_file_path(id)
                if not vod_file_path:
                    warn('No video file for id {}'.format(id))
                    continue


                event_full_name = record.attrib['event_full_name']
                event = record.attrib['event_name']

                if event_full_name != event:
                    dir_path = '{symlink_dir}{game}/{year}/{event}/{season}/{stage}'.format(
                        symlink_dir=symlink_dir, game=get_game(record),
                        year=record.attrib['year'], event=event,
                        season=get_season(record), event_full_name=event_full_name,
                        stage=record.attrib['stage']
                    )
                else:
                    dir_path = '{symlink_dir}{game}/{year}/{event}/{stage}'.format(
                        symlink_dir=symlink_dir, game=get_game(record),
                        year=record.attrib['year'], event=event,
                        stage=record.attrib['stage']
                    )

                file_name = '{match_date} {name}{ext}'.format(
                    match_date=record.attrib['match_date'], name=make_name(record), ext=os.path.splitext(vod_file_path)[1]
                )

                try:
                    os.makedirs(dir_path)
                except OSError as e:
                    if e.errno == 17: # file exists
                        pass
                    else:
                        raise e

                path = dir_path + '/' + file_name
                try:
                    if args.relative:
                            (vod_file_dir, vod_file_name) = os.path.split(vod_file_path)
                            vod_file_path = os.path.relpath(vod_file_dir, dir_path)
                            if vod_file_path[-1] != '/':
                                vod_file_path += '/'

                            vod_file_path += vod_file_name

                    os.symlink(vod_file_path, path)
                    info('Symlink {} to {}'.format(vod_file_path, path))
                except OSError as e:
                    if e.errno == 17: # file exists
                        info('File {} exists'.format(path))
                    else:
                        raise e
                except Exception as e:
                    pass
                # if os.path.isfile(path):
                #     info('File {} exists'.format(path))
                # else:
                #     try:
                #         if args.relative:
                #             (vod_file_dir, vod_file_name) = os.path.split(vod_file_path)
                #             vod_file_path = os.path.relpath(vod_file_dir, dir_path)
                #             if vod_file_path[-1] != '/':
                #                 vod_file_path += '/'

                #             vod_file_path += vod_file_name

                #         os.symlink(vod_file_path, path)
                #         info('Symlink {} to {}'.format(vod_file_path, path))
                #     except Exception as e:
                #         pass

        except Exception as e:
            error('processing file name {}: {}'.format(file_name, str(e)))


    for xml_dir in args.xml_dir:
        if xml_dir[-1] != '/':
            xml_dir += '/'

        xml_dir += '*.xml'
        # for xml_file in glob.iglob(xml_dir):
        #     with open(xml_file, 'rb') as f:
        #         xml = f.read().decode('utf-8')
        #         process_xml(xml_file, xml)

        gzxml_dir = xml_dir + '.gz'
        for gzxml_file in glob.iglob(gzxml_dir):
            info('Processing ' + gzxml_file)
            with gzip.open(gzxml_file, 'rb') as f:
                xml = f.read().decode('utf-8')
                process_xml(gzxml_file, xml)


except Exception as e:
    error('exception ' + str(e))
