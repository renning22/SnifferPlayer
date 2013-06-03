using System;
using System.Collections.Generic;
using System.Text;
using Kaedei.AcDown.Interface;
using System.Text.RegularExpressions;
using System.IO;

namespace Kaedei.AcDown.Downloader
{
    [AcDownPluginInformation("LetvDownloader", "乐视网下载插件", "WeiYuemin", "1.0.0.0", "乐视网下载插件", "http://weibo.com/weiyuemin")]
    public class LetvPlugin : IPlugin
    {

        public LetvPlugin()
        {
            Feature = new Dictionary<string, object>();
            //GetExample
            Feature.Add("ExampleUrl", new string[] { 
				"乐视网(Letv.com)下载插件:",
				"http://www.letv.com/ptv/vplay/383561.html",
				"http://www.letv.com/ptv/pplay/5208/1.html"
			});
        }

        public IDownloader CreateDownloader()
        {
            return new LetvDownloader();
        }

        public bool CheckUrl(string url)
        {
            Regex r = new Regex(@"^http://www\.letv\.com/ptv/(vplay/\d+|pplay/\d+/\d+)\.html$");
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

    public class LetvDownloader : IDownloader
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
            //1.读取播放页面的代码，获取 vid＝1612388 值       http://www.letv.com/ptv/pplay/74497/1.html
            string src = Network.GetHtmlSource(Info.Url, Encoding.UTF8, Info.Proxy);

            Regex r1 = new Regex(@"vid:(?<vid>\d+),");
            Match m1 = r1.Match(src);
            string vid = m1.Groups["vid"].ToString();

            if (string.IsNullOrEmpty(vid))
            {
                return false;
            }

            //2.拼合出信息文件地址        http://www.letv.com/v_xml/1612388.xml
            string step2_src = Network.GetHtmlSource(@"http://www.letv.com/v_xml/" + vid + ".xml", Encoding.UTF8, Info.Proxy);

            //3.从以上文件中读取链接（节点 mmsJson 中的 url 值）　　http://220.181.117.5/ng?s=3&df=17/12/15/13383307831805077.0.flv&br=301
            Regex r2 = new Regex("<mmsJson>.*\"url\":\"" + @"(?<step3_url>.+)" + "\"");
            Match m2 = r2.Match(step2_src);
            string step3_url = m2.Groups["step3_url"].ToString();
            if (string.IsNullOrEmpty(step3_url))
            {
                return false;
            }
            step3_url = step3_url.Replace(@"\/", @"/");
            
            //4.从上面的地址读取信息文件，从中读取 location 的值，舍去所有外挂参数，于下 http://124.232.149.10/17/12/15/13383307831805077.0.letv
            string step4_src = Network.GetHtmlSource(step3_url, Encoding.UTF8, Info.Proxy);
            Regex r3 = new Regex("\"location\":? \"" + @"(?<final_url>.+)\.letv\?.*" + "\"");
            Match m3 = r3.Match(step4_src);

            //5.将扩展名改为 flv ,即为真实链接地址
            string final_src = m3.Groups["final_url"].ToString();
            if (string.IsNullOrEmpty(final_src))
            {
                return false;
            }
            final_src = final_src.Replace(@"\/", @"/") + ".flv";

            Info.videos = new string[1];
            Info.videos[0] = final_src;

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
