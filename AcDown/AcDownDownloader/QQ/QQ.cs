using System;
using System.Collections.Generic;
using System.Text;
using Kaedei.AcDown.Interface;
using System.Text.RegularExpressions;
using System.IO;

namespace Kaedei.AcDown.Downloader
{
    [AcDownPluginInformation("QQDownloader", "腾讯视频下载插件", "WeiYuemin", "1.0.0.0", "腾讯视频下载插件", "http://weibo.com/weiyuemin")]
    public class QQPlugin : IPlugin
    {

        public QQPlugin()
        {
            Feature = new Dictionary<string, object>();
            //GetExample
            Feature.Add("ExampleUrl", new string[] { 
				"腾讯视频(v.qq.com)下载插件:",
				"/vlive.qqvideo.tc.qq.com/l0011rrl3vh.",
				"/vkp.tc.qq.com/K00102hDRZB."
			});
        }

        public IDownloader CreateDownloader()
        {
            return new QQDownloader();
        }

        public bool CheckUrl(string url)
        {
            Regex r = new Regex(@"^/[^/]+/\w{11}\.$");
            Match m = r.Match(url);
            if (m.Success)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        public string GetHash(string url)
        {
            return url;
        }

        public Dictionary<string, object> Feature { get; private set; }

        public SerializableDictionary<string, string> Configuration { get; set; }

    }

    public class QQDownloader : IDownloader
    {

        public TaskInfo Info { get; set; }

        //下载参数
        DownloadParameter currentParameter;

        public DelegateContainer delegates { get; set; }

        //文件总长度
        public long TotalLength
        {
            get
            {
                if (currentParameter != null)
                {
                    return currentParameter.TotalLength;
                }
                else
                {
                    return 0;
                }
            }
        }

        //已完成的长度
        public long DoneBytes
        {
            get
            {
                if (currentParameter != null)
                {
                    return currentParameter.DoneBytes;
                }
                else
                {
                    return 0;
                }
            }
        }

        //最后一次Tick时的值
        public long LastTick
        {
            get
            {
                if (currentParameter != null)
                {
                    //将tick值更新为当前值
                    long tmp = currentParameter.LastTick;
                    currentParameter.LastTick = currentParameter.DoneBytes;
                    return tmp;
                }
                else
                {
                    return 0;
                }
            }
        }


        //下载视频
        public bool Download()
        {
            //1.从发来的地址里取出id，       /vlive.qqvideo.tc.qq.com/l0011rrl3vh. --> l0011rrl3vh
            Regex r1 = new Regex(@"^/[^/]+/(?<id>\w{11})\.$");
            Match m1 = r1.Match(Info.Url);
            string id = m1.Groups["id"].ToString();

            if (string.IsNullOrEmpty(id))
            {
                return false;
            }

            //2.拼合出视频文件地址        http://web.qqvideo.tc.qq.com/l0011rrl3vh.flv
            Info.videos = new string[1];
            Info.videos[0] = "http://web.qqvideo.tc.qq.com/" + id + ".flv";

            return true;
        }


        //停止下载
        public void StopDownload()
        {
            if (currentParameter != null)
            {
                //将停止flag设置为true
                currentParameter.IsStop = true;
            }
        }

    }

}//end namespace
