using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using System.Web.UI;
using System.Web.UI.WebControls;
using System.Text;

public partial class Stats : System.Web.UI.Page
{
    protected void Page_Load(object sender, EventArgs e)
    {
        string A1 = "", A2 = "", A3 = "";
        if (Request.Params["A1"] != null)
        {
            A1 = Request.Params["A1"];
            Response.Write("A1 = " + A1 + "\n");
        }
        if (Request.Params["A2"] != null)
        {
            A2 = Request.Params["A2"];
            Response.Write("A2 = " + A2 + "\n");
        }
        if (Request.Params["A3"] != null)
        {
            A3 = Request.Params["A3"];
            Response.Write("A3 = " + A3 + "\n");
        }
        MySql.Data.MySqlClient.MySqlConnection con = new MySql.Data.MySqlClient.MySqlConnection("Server=42.121.126.238;Database=nnkkdatabase;Uid=root;Pwd=60battle23;CharSet=gb2312");
        con.Open();
        MySql.Data.MySqlClient.MySqlCommand cmd = new MySql.Data.MySqlClient.MySqlCommand("insert into stats (`time`, A1, A2, A3, IP) values (now(), @A1, @A2, @A3, @IP)", con);
        cmd.Parameters.AddWithValue("@A1", A1);
        cmd.Parameters.AddWithValue("@A2", A2);
        cmd.Parameters.AddWithValue("@A3", A3);
        cmd.Parameters.AddWithValue("@IP", Request.UserHostAddress);
        cmd.ExecuteNonQuery();
        con.Close();
    }
}