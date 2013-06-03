using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using Kaedei.AcDown.Core;
using Kaedei.AcDown.Interface;
using Kaedei.AcDown.Downloader;
using System.Text.RegularExpressions;
using System.Text;

public partial class Next : System.Web.UI.Page
{
    protected string GetYoukuNext(string url)
    {
        //1.读取页面的代码       http://v.youku.com/v_show/id_XMjMxODU3MTcy.html
        string src = Network.GetHtmlSource(url, Encoding.UTF8);

        //先看是不是集数模式

        //这个要判严格点，避免list匹住多余的不相干的东西

        //结束的地方有两种情况，一种是有翻页的，一种是没有翻页的
        Regex r_pack_number_1 = new Regex("<!--集数模式-->\\s+<ul class=\"pack_number\">(?<list>.+)</ul>\\s+<div class=\"clear\"></div>\\s+(<div class=\"qPager qPager_small\"|</div>\\s+</div>)", RegexOptions.Singleline);
        Match m_pack_number_1 = r_pack_number_1.Match(src);
        string list = m_pack_number_1.Groups["list"].ToString();

        if (!string.IsNullOrEmpty(list))
        {
            Regex r_pack_number_2 = new Regex("<li class=\"current\">.+</li>\\s+<li><div class=\"sn\"><a .*title=\"(?<title>.+)\" .*href=\"(?<url>[^\"]+)\"");
            Match m_pack_number_2 = r_pack_number_2.Match(list);
            string next_url = m_pack_number_2.Groups["url"].ToString();

            if (string.IsNullOrEmpty(next_url))
            {
                //可能是本页最后一集，尝试翻页
                Regex r_pack_number_3 = new Regex("<li title=\"下一页\" class=\"next\"><a .*href=\"(?<next_page>/(v_vpofficialsegment|v_vpofficiallist)/[^\"]+)\"");
                Match m_pack_number_3 = r_pack_number_3.Match(src);
                string next_page = m_pack_number_3.Groups["next_page"].ToString();

                if (string.IsNullOrEmpty(next_page))
                {
                    return null;
                }

                string next_page_src = Network.GetHtmlSource("http://v.youku.com" + next_page + "?__rt=1&__ro=listShow", Encoding.UTF8);

                Regex r_pack_number_4 = new Regex("<ul class=\"pack_number\">\\s+<li><div class=\"sn\"><a .*title=\"(?<title>.+)\" .*href=\"(?<url>[^\"]+)\"");
                Match m_pack_number_4 = r_pack_number_4.Match(next_page_src);
                string next_page_first_url = m_pack_number_4.Groups["url"].ToString();

                return next_page_first_url;
            }
            else
            {
                return next_url;
            }
        }
        else
        {
            //再看是不是列表模式

            //列表模式的这种没找到不带翻页的，假设不带翻页的代码和剧集列表的一样
            Regex r_pack_list_1 = new Regex("<ul class=\"pack_list\">(?<list>.+)</ul>\\s+<div class=\"clear\"></div>\\s+(<div class=\"qPager qPager_small\"|</div>\\s+</div>)", RegexOptions.Singleline);
            Match m_pack_list_1 = r_pack_list_1.Match(src);
            string list2 = m_pack_list_1.Groups["list"].ToString();

            if (!string.IsNullOrEmpty(list2))
            {
                Regex r_pack_list_2 = new Regex("<li class=\"current\" >\\s+.+\\s+.+\\s+</li>\\s+<li >\\s+<div class=\"show_title\" title=\"(?<title>.+)\"\\s?><a .*href=\"(?<url>[^\"]+)\"");
                Match m_pack_list_2 = r_pack_list_2.Match(list2);
                string next_url = m_pack_list_2.Groups["url"].ToString();

                if (string.IsNullOrEmpty(next_url))
                {
                    //可能是本页最后一集，尝试翻页
                    Regex r_pack_list_3 = new Regex("<li title=\"下一页\" class=\"next\"><a .*href=\"(?<next_page>/(v_vpofficialsegment|v_vpofficiallist)/[^\"]+)\"");
                    Match m_pack_list_3 = r_pack_list_3.Match(src);
                    string next_page = m_pack_list_3.Groups["next_page"].ToString();

                    if (string.IsNullOrEmpty(next_page))
                    {
                        return null;
                    }

                    string next_page_src = Network.GetHtmlSource("http://v.youku.com" + next_page + "?__rt=1&__ro=listShow", Encoding.UTF8);

                    Regex r_pack_list_4 = new Regex("<ul class=\"pack_list\">\\s+<li [^>]*>\\s+<div class=\"show_title\" title=\"(?<title>.+)\"\\s?><a .*href=\"(?<url>[^\"]+)\"");
                    Match m_pack_list_4 = r_pack_list_4.Match(next_page_src);
                    string next_page_first_url = m_pack_list_4.Groups["url"].ToString();

                    return next_page_first_url;
                }
                else
                {
                    return next_url;
                }
            }
            else
            {
                //都不满足再当普通视频处理

                Regex r1 = new Regex("<div class=\"body\">(\r|)\n<div class=\"vRelated\">(\r|)\n<div class=\"collgrid\\w{2}\">(\r|)\n<div class=\"items\">(?<related>.+)</div>(\r|)\n</div></div></div>", RegexOptions.Singleline);
                Match m1 = r1.Match(src);
                string related = m1.Groups["related"].ToString();

                if (string.IsNullOrEmpty(related))
                {
                    return null;
                }

                MatchCollection mcKey = Regex.Matches(related, "<li class=\"v_title\"><a href=\"(?<url>[^\"]+)\" target=\"video\" title=\"(?<title>.+)\"");
                List<string> urls = new List<string>();
                List<string> titles = new List<string>();
                foreach (Match mKey in mcKey)
                {
                    urls.Add(mKey.Groups["url"].Value);
                    titles.Add(mKey.Groups["title"].Value);
                }

                if (urls.Count > 0)
                {
                    Random r = new Random();
                    return urls[r.Next(urls.Count)];
                }
                else
                {
                    return null;
                }
            }
        }
    }

    protected void Page_Load(object sender, EventArgs e)
    {
        bool error = true;
        if (Request.QueryString["url"] != null)
        {
            //string url = "http://v.youku.com/v_show/id_XNDUxNjU4NjAw.html"; //集数模式
            //string url = "http://v.youku.com/v_show/id_XNDQzNDY0MDg4.html"; //集数模式 本页最后一集
            //string url = "http://v.youku.com/v_show/id_XNDQ3OTE5MzE2.html"; //列表模式 类型1
            //string url = "http://v.youku.com/v_show/id_XNDUzNzE5OTg0.html"; //列表模式 类型2
            //string url = "http://v.youku.com/v_show/id_XNDQ3OTIyNTg0.html"; //列表模式 类型1 本页最后一段
            //string url = "http://v.youku.com/v_show/id_XNDUzNzM1NDg0.html"; //列表模式 类型2 本页最后一集
            //string url = "http://v.youku.com/v_show/id_XMzEyNjQxNjk2.html"; //普通
            string url = Request.QueryString["url"];
            if (new YoukuPlugin().CheckUrl(url))
            {
                string next_url = GetYoukuNext(url);
                if (! string.IsNullOrEmpty(next_url))
                {
                    error = false;
                    Response.Write(next_url);
                }
            }
        }

        if (error)
        {
            Response.Write("http://v.youku.com/v_show/id_XNDUzNzE5OTg0.html"); //没有推荐或者出错时返回的默认视频
        }
    }
}