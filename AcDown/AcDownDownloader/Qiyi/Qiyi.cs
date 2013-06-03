using System;
using System.Collections.Generic;
using System.Text;
using Kaedei.AcDown.Interface;
using System.Text.RegularExpressions;
using System.IO;

namespace Kaedei.AcDown.Downloader
{
    [AcDownPluginInformation("QiyiDownloader", "奇艺网下载插件", "WeiYuemin", "1.0.0.0", "奇艺网下载插件", "http://weibo.com/weiyuemin")]
    public class QiyiPlugin : IPlugin
    {

        public QiyiPlugin()
        {
            Feature = new Dictionary<string, object>();
            //GetExample
            Feature.Add("ExampleUrl", new string[] { 
				"奇艺网(iqiyi.com)下载插件:",
				"http://www.qiyi.com/dianshiju/20110418/48af82e3012faac7.html",
				"http://www.iqiyi.com/zongyi/20120826/75562ced3704d539.html"
			});
        }

        public IDownloader CreateDownloader()
        {
            return new QiyiDownloader();
        }

        public bool CheckUrl(string url)
        {
            Regex r = new Regex(@"^cache\.video\.qiyi\.com/vd/\d{1,8}/\w{32}/$");
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

    public class QiyiDownloader : IDownloader
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
            //1.从客户端发来的/vd/<tvId>/<videoId>/中提取videoId: 6d81c5c94e5649e88dcd07428e567198
            Regex r1 = new Regex("^cache\\.video\\.qiyi\\.com/vd/\\d{1,8}/(?<vid>\\w{32})/$");
            Match m1 = r1.Match(Info.Url);
            string vid = m1.Groups["vid"].ToString();

            if (string.IsNullOrEmpty(vid))
            {
                return false;
            }

            //2.拼合出信息文件地址，获取内容        http://cache.video.qiyi.com/m/6d81c5c94e5649e88dcd07428e567198/
            string step2_src = Network.GetHtmlSource(@"http://cache.video.qiyi.com/m/" + vid + "//", Encoding.UTF8, Info.Proxy);

            //3.从信息文件中得到  "mp4Url":"http://data.video.qiyi.com/videos/tv/20120827/e5775ea93357f60b342423971fe138ca.mp4"
            Regex r2 = new Regex("\"mp4Url\":\"(?<mp4url>[^\"]+)\"");
            Match m2 = r2.Match(step2_src);
            string step3_url = m2.Groups["mp4url"].ToString();
            if (string.IsNullOrEmpty(step3_url))
            {
                return false;
            }

            //4.再从mp4Url的内容中取地址。假设只有一个文件 data:{"l":"<url>"，因为移动端一般都只有一个文件
            string step4_src = Network.GetHtmlSource(step3_url, Encoding.UTF8, Info.Proxy);

            Regex r3 = new Regex("data:{\"l\":\"(?<final_url>[^\"]+)\"");
            Match m3 = r3.Match(step4_src);
            string final_url = m3.Groups["final_url"].ToString();
            if (string.IsNullOrEmpty(final_url))
            {
                return false;
            }

            Info.videos = new string[1];
            Info.videos[0] = final_url;

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
