using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using Kaedei.AcDown.Core;
using Kaedei.AcDown.Interface;
using System.IO;
using System.Collections.ObjectModel;
using Kaedei.AcDown.Downloader;

public partial class _Default : System.Web.UI.Page
{
    public void empty_delegate(DelegateParameter para)
    {
    }

    protected void Page_Load(object sender, EventArgs e)
    {
        //官方插件
        var plugins = new Collection<IPlugin>() 
				{ 
					new AcFunPlugin(), 
					new YoukuPlugin(),
					new YouTubePlugin(),
                    new LetvPlugin(),
                    new QiyiPlugin(),
                    new QQPlugin(),
                    new SinaPlugin(),
					new NicoPlugin(),
					new BilibiliPlugin(),
					new TudouPlugin(),
					new ImanhuaPlugin(),
					new TiebaAlbumPlugin(),
					new SfAcgPlugin(),
					new TucaoPlugin(),
					new FlvcdPlugin()
				};

        //初始化核心
        CoreManager.Initialize(
             Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), @"Kaedei\AcDown\"),
             new UIDelegateContainer(null, null, null, null, null, null, null, null),
             plugins);
        
        string url;
        if (Request.QueryString["url"] == null)
        {
            url = "http://v.youku.com/v_show/id_XNDE5NjE4NjI0.html";
            //url = "http://v.youku.com/v_show/id_XNDQyMDQ5Mzg0.html";
            //url = "http://www.tudou.com/albumplay/T32z2sgr3Uk";
            //url = "http://www.tudou.com/programs/view/ShtKrHZ5O_8/?fr=rec2";
            //url = "http://www.tudou.com/albumplay/zA8Duvz4eUg/8_KTqOUSonI.html";
            //url = "http://www.tudou.com/listplay/69gOmmDlugI/JJKorVrTgPk.html"; 
            //url = "http://www.tudou.com/listplay/axxbYWyQhT8/fzaY9-d0Stw.html"; 
            //url = "http://www.letv.com/ptv/pplay/79146/98.html";
            //url = "http://www.letv.com/ptv/vplay/1696569.html";
            //url = "http://v.youku.com/v_show/id_XMjI3MDI1NTM2.html";
            //url = "cache.video.qiyi.com/vd/231669/07f84458488542a2b41f79db3422b335/"; // <-- already broken
            //url = "cache.video.qiyi.com/vd/88373/ce7d6a8f76b1484faa17c8ccaaae225d/";
            //url = "/vkp.tc.qq.com/K00102hDRZB.";
            //url = "/v_play.php?vid=84716144&";
            //url = "http://www.56.com/w99/play_album-aid-10025226_vid-Njk4MDkzOTQ.html";
        }
        else
        {
            url = Request.QueryString["url"].ToString();
        }

        IPlugin plugin = null;

        //查找所有候选插件
        foreach (var item in CoreManager.PluginManager.Plugins)
        {
            if (item.CheckUrl(url))
            {
                plugin = item;
                break;
            }
        }

        if (plugin != null)
        {
            TaskInfo task = new TaskInfo();
            task.Url = url;
            task.SourceUrl = url;
            task.BasePlugin = plugin;

            task.AutoAnswer = new List<AutoAnswer>();

            if (task.BasePlugin.Feature.ContainsKey("AutoAnswer"))
            {
                task.AutoAnswer = (List<AutoAnswer>)task.BasePlugin.Feature["AutoAnswer"];
            }
            task.SaveDirectory = new DirectoryInfo("/");

            IDownloader resourceDownloader = task.BasePlugin.CreateDownloader();
            resourceDownloader.delegates = new DelegateContainer(empty_delegate, empty_delegate, empty_delegate, empty_delegate);
            resourceDownloader.Info = task;

            if (resourceDownloader.Download() && task.videos != null)
            {
                foreach (string fp in task.videos)
                {
                    Response.Write(fp + "\n");
                }
            }
            else
            {
                Response.Write("Sorry");
            }
        }
        else
        {
            Response.Write("NoSuitablePlugin");
        }

        //Response.Write(url);
    }
}
