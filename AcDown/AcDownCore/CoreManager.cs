﻿
using System.IO;
using System.Collections.ObjectModel;
using Kaedei.AcDown.Interface;
namespace Kaedei.AcDown.Core
{
	/// <summary>
	/// AcDown核心管理器
	/// </summary>
	public class CoreManager
	{
		private CoreManager() { }

		/// <summary>
		/// 起始路径
		/// </summary>
		public static string StartupPath { get; private set; }
		/// <summary>
		/// UI委托
		/// </summary>
		public static UIDelegateContainer UIDelegates { get; private set; }
		/// <summary>
		/// 插件管理器
		/// </summary>
		public static PluginManager PluginManager { get; private set; }
		/// <summary>
		/// 任务管理器
		/// </summary>
		public static TaskManager TaskManager { get; private set; }
		/// <summary>
		/// 配置管理器
		/// </summary>
		public static ConfigManager ConfigManager { get; private set; }

		/// <summary>
		/// 初始化AcDown核心
		/// </summary>
		/// <param name="startupFolderPath">起始路径</param>
		/// <param name="uiDelegates">UI委托</param>
		public static void Initialize(string startupFolderPath, UIDelegateContainer uiDelegates)
		{
			Initialize(startupFolderPath, uiDelegates);
		}

		/// <summary>
		/// 初始化AcDown核心
		/// </summary>
		/// <param name="startupFolderPath">起始路径</param>
		/// <param name="uiDelegates">UI委托</param>
		/// <param name="externalPlugins">额外加载的内部插件</param>
		public static void Initialize(string startupFolderPath, UIDelegateContainer uiDelegates,
			Collection<IPlugin> internalPlugins)
		{
            /* 2012.08.27修改　不访问本地目录
			StartupPath = startupFolderPath;
			//如果目录不存在则创建
			if (!Directory.Exists(startupFolderPath))
			{
				Directory.CreateDirectory(startupFolderPath);
			}
            */
			//全局设置
			ConfigManager = new ConfigManager();
            ConfigManager.LoadSettings();　
			//记录
			Logging.Initialize();
			//插件管理器
			PluginManager = new PluginManager();
			PluginManager.LoadPlugins();
			foreach (IPlugin plugin in internalPlugins)
			{
				PluginManager.LoadPlugin(plugin);
			}
			//委托
			UIDelegates = uiDelegates;

            // 2012.08.27修改 不从硬盘里读取旧任务
			//任务管理器
			//TaskManager = new TaskManager();
			//TaskManager.LoadAllTasks();
		}

	}
}
