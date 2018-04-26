import { Component, OnInit } from '@angular/core';
import {TranslateService} from '@ngx-translate/core';
import { Router } from '@angular/router';

@Component({
  selector: 'app-nav',
  templateUrl: './nav.component.html',
  styleUrls: ['./nav.component.css']
})
export class NavComponent implements OnInit {

  appConfig: any;
  menuItems: any[];
  progressBarMode: string;
  currentLang: string;

  activeTab = 1;
  activeLink: any;
  tabs: any[] = [];
  routeLinks = [
    {
      icon: 'home',
      label: 'Overview',
      link: '/overview',
      tab: 'Monitoring'
    }, {
      icon: 'code',
      label: 'Queries',
      link: '/queries',
      tab: 'Monitoring'
    }, {
      icon: 'thermometer-half',
      label: 'Metrics',
      link: '/metrics',
      tab: 'Monitoring'
    }, {
      icon: 'file',
      label: 'Log',
      link: '/log',
      tab: 'Monitoring'
    }, {
      icon: 'cubes',
      label: 'Modules',
      link: '/modules',
      tab: 'Settings'
    }, {
      icon: 'cog',
      label: 'Settings',
      link: '/settings',
      tab: 'Settings'
    }
  ];

  constructor(private translateService: TranslateService, 
    private router: Router) {
    translateService.addLangs(['en']);
    translateService.setDefaultLang('en');

    var tabs = new Map<string,any>();
    var index = 0;
    for (let l of this.routeLinks) {
      if (!tabs.has(l.tab)) {
        tabs.set(l.tab, {
          tab: l.tab,
          links: [],
          index: index
        });
        index++;
      }
      tabs.get(l.tab).links.push(l);
    }
    this.tabs = Array.from(tabs.values());
  }

  ngOnInit(): void {
    this.router.events.subscribe((res) => {
      this.activeLink = this.routeLinks.find(tab => tab.link == this.router.url);
      if (this.activeLink) {
        this.activeTab = this.tabs.indexOf(this.tabs.find(t => t.tab == this.activeLink.tab));
      }
    });
  }


}
