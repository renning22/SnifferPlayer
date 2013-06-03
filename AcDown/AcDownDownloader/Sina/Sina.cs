using System;
using System.Collections.Generic;
using System.Text;
using Kaedei.AcDown.Interface;
using System.Text.RegularExpressions;
using System.IO;

namespace Kaedei.AcDown.Downloader
{
    [AcDownPluginInformation("SinaDownloader", "新浪视频下载插件", "WeiYuemin", "1.0.0.0", "新浪视频下载插件", "http://weibo.com/weiyuemin")]
    public class SinaPlugin : IPlugin
    {

        public SinaPlugin()
        {
            Feature = new Dictionary<string, object>();
            //GetExample
            Feature.Add("ExampleUrl", new string[] { 
				"新浪视频(video.sina.com.cn)下载插件:",
				"/v_play.php?vid=82642019&"
			});
        }

        public IDownloader CreateDownloader()
        {
            return new SinaDownloader();
        }

        public bool CheckUrl(string url)
        {
            Regex r = new Regex(@"^/v_play\.php\?vid=\d+$");
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

    public class SinaDownloader : IDownloader
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
            //1.从发来的地址里取出id，       /v_play.php?vid=82642019& --> 82642019
            Regex r1 = new Regex(@"^/v_play\.php\?vid=(?<vid>\d+)$");
            Match m1 = r1.Match(Info.Url);
            string vid = m1.Groups["vid"].ToString();

            if (string.IsNullOrEmpty(vid))
            {
                return false;
            }

            //2.传给Parser
            SinaVideoParser parserSina = new SinaVideoParser();
            ParseResult pr = parserSina.Parse(new ParseRequest() { Id = vid, Proxy = Info.Proxy, AutoAnswers = Info.AutoAnswer });
            Info.videos = pr.ToArray();

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
